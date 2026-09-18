#include "vanetza_btp_adapter.h"

#include "vanetza_integration/core/v2x_log.h"

#include <vanetza/btp/data_request.hpp>
#include <vanetza/btp/data_interface.hpp>
#include <vanetza/btp/header.hpp>
#include <vanetza/btp/header_conversion.hpp>
#include <vanetza/btp/port_dispatcher.hpp>

#include <vanetza/net/chunk_packet.hpp>
#include <vanetza/net/cohesive_packet.hpp>
#include <vanetza/net/osi_layer.hpp>

#include <vanetza/common/byte_buffer.hpp>
#include <vanetza/common/its_aid.hpp>

#include <vanetza/geonet/data_confirm.hpp>
#include <vanetza/geonet/areas.hpp>
#include <vanetza/geonet/data_request.hpp>
#include <vanetza/geonet/interface.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/geonet/traffic_class.hpp>

#include <boost/variant/get.hpp>

#include "ns3/log.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ns3 {

namespace {

const char* ResultCodeToString(
    vanetza::geonet::DataConfirm::ResultCode rc)
{
    using RC = vanetza::geonet::DataConfirm::ResultCode;

    switch (rc)
    {
        case RC::Accepted:
            return "Accepted";

        case RC::Rejected_Max_SDU_Size:
            return "Rejected_Max_SDU_Size";

        case RC::Rejected_Max_Lifetime:
            return "Rejected_Max_Lifetime";

        case RC::Rejected_Min_Repetition_Interval:
            return "Rejected_Min_Repetition_Interval";

        case RC::Rejected_Unsupported_Traffic_Class:
            return "Rejected_Unsupported_Traffic_Class";

        case RC::Rejected_Max_Geo_Area_Size:
            return "Rejected_Max_Geo_Area_Size";

        case RC::Rejected_Unspecified:
            return "Rejected_Unspecified";
    }

    return "UNKNOWN";
}

} // anonymous namespace


//=============================================================================
// Internal BTP RX indication handler
//
// This class is intentionally private to VanetzaBtpAdapter.
//
// It is the Vanetza-facing part of the BTP RX boundary.
// No Vanetza BTP type is exposed to the application.
//=============================================================================

class VanetzaBtpAdapter::RxIndicationHandler
    : public vanetza::btp::IndicationInterface
{
public:

    explicit RxIndicationHandler(VanetzaBtpAdapter& owner)
        : m_owner(owner)
    {
    }

    void indicate(
        const vanetza::btp::DataIndication& ind,
        std::unique_ptr<vanetza::UpPacket> packet) override
    {
        const uint16_t destinationPort =
            ind.destination_port.get();

        NS_LOG_UNCOND(
            "[BTP_HANDLER] indicate() called port="
            << destinationPort);

        const auto handlerIt =
            m_owner.m_handlers.find(destinationPort);

        if (handlerIt == m_owner.m_handlers.end())
        {
            NS_LOG_UNCOND(
                "[BTP_HANDLER] ERROR: RX handler not installed "
                "for port="
                << destinationPort);
            return;
        }

        RxHandler handler =
            handlerIt->second;

        if (!packet)
        {
            NS_LOG_UNCOND(
                "[BTP_HANDLER] ERROR: UpPacket is null");
            return;
        }

        //----------------------------------------------------------------------
        // Create application-neutral RX result.
        //----------------------------------------------------------------------
        VanetzaBtpAdapter::RxData data;

        data.destinationPort =
            ind.destination_port.get();

        //----------------------------------------------------------------------
        // Extract application payload from Vanetza UpPacket.
        //----------------------------------------------------------------------
        vanetza::ByteBuffer payload;

        if (auto* chunk =
                boost::get<vanetza::ChunkPacket>(&*packet))
        {
            chunk->layer(
                vanetza::OsiLayer::Application).convert(payload);
        }
        else if (auto* cohesive =
                     boost::get<vanetza::CohesivePacket>(&*packet))
        {
            auto net =
                (*cohesive)[vanetza::OsiLayer::Network];

            auto tr =
                (*cohesive)[vanetza::OsiLayer::Transport];

            auto ses =
                (*cohesive)[vanetza::OsiLayer::Session];

            auto pre =
                (*cohesive)[vanetza::OsiLayer::Presentation];

            auto app =
                (*cohesive)[vanetza::OsiLayer::Application];

            NS_LOG_UNCOND(
                "[BTP_HANDLER] Network bytes    = "
                << std::distance(net.begin(), net.end()));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] size(Network)="
                << cohesive->size(vanetza::OsiLayer::Network));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] Transport bytes  = "
                << std::distance(tr.begin(), tr.end()));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] size(Transport)="
                << cohesive->size(vanetza::OsiLayer::Transport));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] Session bytes    = "
                << std::distance(ses.begin(), ses.end()));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] size(Session)="
                << cohesive->size(vanetza::OsiLayer::Session));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] Presentation bytes = "
                << std::distance(pre.begin(), pre.end()));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] size(Presentation)="
                << cohesive->size(vanetza::OsiLayer::Presentation));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] Application bytes = "
                << std::distance(app.begin(), app.end()));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] size(Application)="
                << cohesive->size(vanetza::OsiLayer::Application));

            NS_LOG_UNCOND(
                "[BTP_HANDLER] total="
                << cohesive->size());

            //------------------------------------------------------------------
            // Preserve the existing application payload extraction behavior.
            //
            // In the current Vanetza RX path, after BTP processing the CAM
            // payload is carried in the Session layer.
            //------------------------------------------------------------------
            payload.assign(
                ses.begin(),
                ses.end());
        }
        else
        {
            NS_LOG_UNCOND(
                "[BTP_HANDLER] ERROR: Unknown packet type in UpPacket");
            return;
        }

        //----------------------------------------------------------------------
        // Validate extracted payload.
        //----------------------------------------------------------------------
        if (payload.empty())
        {
            NS_LOG_UNCOND(
                "[BTP_HANDLER] WARNING: Extracted payload is empty");
            return;
        }

        data.payload.assign(
            payload.begin(),
            payload.end());

        NS_LOG_UNCOND(
            "[BTP_HANDLER] Payload extracted, size="
            << data.payload.size());

        //----------------------------------------------------------------------
        // GeoNetworking source position.
        //----------------------------------------------------------------------
        data.hasSourcePosition = true;

        data.latitude =
            ind.source_position.latitude.value() * 1e-7;

        data.longitude =
            ind.source_position.longitude.value() * 1e-7;

        //----------------------------------------------------------------------
        // GeoNetworking transport information.
        //----------------------------------------------------------------------
        data.hasTrafficClass = true;

        data.trafficClass =
            ind.traffic_class.raw();

        //----------------------------------------------------------------------
        // ITS-AID.
        //----------------------------------------------------------------------
        data.hasItsAid = false;

        if (ind.its_aid)
        {
            data.hasItsAid = true;
            data.itsAid = ind.its_aid.value();
        }

        //----------------------------------------------------------------------
        // Permissions.
        //----------------------------------------------------------------------
        data.hasPermissions = false;

        if (ind.permissions)
        {
            data.hasPermissions = true;
            data.permissionsSize =
                ind.permissions->size();
        }

        //----------------------------------------------------------------------
        // Remaining lifetime.
        //----------------------------------------------------------------------
        data.hasLifetime = false;

        if (ind.remaining_packet_lifetime)
        {
            data.hasLifetime = true;

            auto duration =
                ind.remaining_packet_lifetime->decode();

            data.lifetimeMs =
                duration.value() * 1000.0;
        }

        //----------------------------------------------------------------------
        // GeoNetworking sender MID.
        //
        // Vanetza representation stays inside this boundary.
        // The application receives only std::array<uint8_t, 6>.
        //----------------------------------------------------------------------
        const auto& sourceMid =
            ind.source_position.gn_addr.mid();

        for (std::size_t i = 0; i < 6; ++i)
        {
            data.sourceMid[i] =
                sourceMid.octets[i];
        }

        data.hasSourceMid = true;

        //----------------------------------------------------------------------
        // Deliver neutral RX data to the application/runtime.
        //----------------------------------------------------------------------
        handler(data);
    }

private:

    VanetzaBtpAdapter& m_owner;
};


//=============================================================================
// Constructor
//
// The BTP PortDispatcher belongs entirely to this wrapper.
//=============================================================================

VanetzaBtpAdapter::VanetzaBtpAdapter(
    vanetza::geonet::Router& router,
    const vanetza::geonet::MIB& mib)
    : m_router(router),
      m_mib(mib),
      m_dispatcher(
          std::make_unique<vanetza::btp::PortDispatcher>()),
      m_rxHandler(
          std::make_unique<RxIndicationHandler>(*this))
{
    //-------------------------------------------------------------------------
    // Undispatched BTP diagnostics.
    //-------------------------------------------------------------------------
    m_dispatcher->hook_undispatched =
        [](const vanetza::geonet::DataIndication&,
           const vanetza::btp::DataIndication* btp)
    {
        if (btp)
        {
            NS_LOG_UNCOND(
                "[BTP_UNDISPATCHED] No handler for port="
                << btp->destination_port
                << " src_port="
                << (btp->source_port
                        ? std::to_string(
                              btp->source_port->get())
                        : "null"));
        }
        else
        {
            NS_LOG_UNCOND(
                "[BTP_UNDISPATCHED] "
                "BTP header parsing FAILED "
                "(parse_btp_header returned none)");
        }
    };

    NS_LOG_UNCOND(
        "[BTP_RX] hook_undispatched installed");

    //-------------------------------------------------------------------------
    // Install this adapter as the BTP-B transport handler of GeoNetworking.
    //-------------------------------------------------------------------------
    m_router.set_transport_handler(
        vanetza::geonet::UpperProtocol::BTP_B,
        m_dispatcher.get());

    NS_LOG_UNCOND(
        "[BTP_RX] Router transport handler = BTP_B");
}


//=============================================================================
// Destructor
//=============================================================================

VanetzaBtpAdapter::~VanetzaBtpAdapter() = default;


//=============================================================================
// Install application RX handler for one destination port.
//=============================================================================

void
VanetzaBtpAdapter::SetRxHandler(
    uint16_t destinationPort,
    RxHandler handler)
{
    if (!handler)
    {
        NS_LOG_UNCOND(
            "[BTP_RX] ERROR: null RX handler for port="
            << destinationPort);
        return;
    }

    m_handlers[destinationPort] =
        std::move(handler);

    m_dispatcher->set_non_interactive_handler(
        vanetza::btp::port_type(destinationPort),
        m_rxHandler.get());

    NS_LOG_UNCOND(
        "[BTP_RX] Non-interactive handler installed port="
        << destinationPort);
}


//=============================================================================
// BTP TX
//=============================================================================

bool
VanetzaBtpAdapter::SendBtpB(
    PacketContext& context,
    const BtpTxRequest& txRequest,
    const std::vector<uint8_t>& payload)
{
    context.AdvancePipeline(PipelineStage::BTP_TX);

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "BTP",
        "BTP_TX_BEGIN",
        "port=" + std::to_string(txRequest.destinationPort) +
        " payload=" + std::to_string(payload.size()));

    //----------------------------------------------------------------------
    // Validate neutral request at the boundary.
    //----------------------------------------------------------------------

    if (txRequest.destinationPort == 0 ||
        txRequest.itsAid == 0)
    {
        context.MarkFailure();
        context.reason = "invalid_btp_tx_request";

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "BTP",
            "BTP_TX_FAIL",
            "invalid destination port or ITS-AID");

        return false;
    }

    if (txRequest.communicationProfile !=
        BtpCommunicationProfile::ITS_G5)
    {
        context.MarkFailure();
        context.reason = "unsupported_communication_profile";

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "BTP",
            "BTP_TX_FAIL",
            "unsupported communication profile");

        return false;
    }

    if (txRequest.transport == BtpTransportType::SHB)
    {
        if (txRequest.destinationArea.has_value())
        {
            context.MarkFailure();
            context.reason = "SHB_with_destination_area";

            V2XWriteLog(
                context,
                V2XLogLevel::ERROR,
                "BTP",
                "BTP_TX_FAIL",
                "SHB request must not contain destination area");

            return false;
        }
    }
    else if (txRequest.transport == BtpTransportType::GBC)
    {
        if (!txRequest.destinationArea.has_value())
        {
            context.MarkFailure();
            context.reason = "GBC_without_destination_area";

            V2XWriteLog(
                context,
                V2XLogLevel::ERROR,
                "BTP",
                "BTP_TX_FAIL",
                "GBC request requires destination area");

            return false;
        }

        const auto& area =
            *txRequest.destinationArea;

        if (!std::isfinite(area.latitudeDeg) ||
            !std::isfinite(area.longitudeDeg) ||
            !std::isfinite(area.radiusMeters) ||
            area.radiusMeters <= 0.0)
        {
            context.MarkFailure();
            context.reason = "invalid_gbc_area";

            V2XWriteLog(
                context,
                V2XLogLevel::ERROR,
                "BTP",
                "BTP_TX_FAIL",
                "invalid GBC destination area");

            return false;
        }
    }

    //----------------------------------------------------------------------
    // BTP-B request
    //----------------------------------------------------------------------

    vanetza::btp::DataRequestB request;

    request.destination_port =
        vanetza::uint16be_t(
            txRequest.destinationPort);

    request.destination_port_info =
        vanetza::uint16be_t(
            txRequest.destinationPortInfo);

    request.gn.communication_profile =
        vanetza::geonet::CommunicationProfile::ITS_G5;

    request.gn.its_aid =
        txRequest.itsAid;

    request.gn.traffic_class =
        vanetza::geonet::TrafficClass(
            txRequest.trafficClass);

    //----------------------------------------------------------------------
    // Select GeoNetworking transport.
    //
    // This decision is based on the transport request, never on the BTP
    // destination port.
    //----------------------------------------------------------------------

    if (txRequest.transport == BtpTransportType::SHB)
    {
        request.gn.transport_type =
            vanetza::geonet::TransportType::SHB;

        request.gn.destination =
            std::nullptr_t{};

        context.MarkSuccess();

        V2XWriteLog(
            context,
            V2XLogLevel::DEBUG,
            "BTP",
            "BTP_TX_OK",
            "port=" +
                std::to_string(txRequest.destinationPort) +
            " transport=SHB aid=" +
                std::to_string(txRequest.itsAid));
    }
    else
    {
        const auto& area =
            *txRequest.destinationArea;

        request.gn.transport_type =
            vanetza::geonet::TransportType::GBC;

        vanetza::geonet::Area destination;

        destination.position =
            vanetza::geonet::GeodeticPosition(
                vanetza::units::GeoAngle::from_value(
                    area.latitudeDeg),
                vanetza::units::GeoAngle::from_value(
                    area.longitudeDeg));

        vanetza::geonet::Circle circle;

        circle.r =
            vanetza::units::Length::from_value(
                area.radiusMeters);

        destination.shape = circle;

        destination.angle =
            vanetza::units::Angle::from_value(0.0);

        request.gn.destination =
            destination;

        context.MarkSuccess();

        V2XWriteLog(
            context,
            V2XLogLevel::DEBUG,
            "BTP",
            "BTP_TX_OK",
            "port=" +
                std::to_string(txRequest.destinationPort) +
            " transport=GBC aid=" +
                std::to_string(txRequest.itsAid));
    }

    //----------------------------------------------------------------------
    // GEONET_TX
    //----------------------------------------------------------------------

    context.AdvancePipeline(PipelineStage::GEONET_TX);

    const char* transportName =
        txRequest.transport == BtpTransportType::SHB
            ? "SHB"
            : "GBC";

    const char* aidName =
        txRequest.itsAid == vanetza::aid::CA
            ? "CA"
            : (txRequest.itsAid == vanetza::aid::DEN
                   ? "DEN"
                   : "OTHER");

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "GEONET",
        "GEONET_TX_BEGIN",
        std::string("transport=") +
            transportName +
            " profile=ITS_G5 aid=" +
            aidName);

    //----------------------------------------------------------------------
    // Build the appropriate GeoNetworking request.
    //----------------------------------------------------------------------

    vanetza::btp::HeaderB btpHeader;

    btpHeader.destination_port =
        request.destination_port;

    btpHeader.destination_port_info =
        request.destination_port_info;

    vanetza::ByteBuffer appPayload(
        payload.begin(),
        payload.end());

    auto packet =
        std::make_unique<vanetza::DownPacket>();

    packet->layer(
        vanetza::OsiLayer::Transport) =
        btpHeader;

    packet->layer(
        vanetza::OsiLayer::Application) =
        std::move(appPayload);

    vanetza::geonet::DataConfirm confirm =
        [&]() -> vanetza::geonet::DataConfirm
    {
        if (txRequest.transport == BtpTransportType::SHB)
        {
            vanetza::geonet::ShbDataRequest gnRequest(m_mib);

            vanetza::geonet::copy_request_parameters(
                request,
                gnRequest);

            gnRequest.upper_protocol =
                vanetza::geonet::UpperProtocol::BTP_B;

            return m_router.request(
                gnRequest,
                std::move(packet));
        }

        vanetza::geonet::GbcDataRequest gnRequest(m_mib);

        vanetza::geonet::copy_request_parameters(
            request,
            gnRequest);

        gnRequest.upper_protocol =
            vanetza::geonet::UpperProtocol::BTP_B;

        return m_router.request(
            gnRequest,
            std::move(packet));
    }();

    context.MarkSuccess();

    V2XWriteLog(
        context,
        V2XLogLevel::DEBUG,
        "GEONET",
        "GEONET_TX_OK",
        std::string("transport=") +
            transportName +
            " profile=ITS_G5 aid=" +
            aidName);

    //----------------------------------------------------------------------
    // Router confirmation
    //----------------------------------------------------------------------

    if (!confirm.accepted())
    {
        context.MarkFailure();

        context.reason =
            ResultCodeToString(
                confirm.result_code);

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "GEONET",
            "GEONET_TX_FAIL",
            "result=" + context.reason);

        V2XWriteLog(
            context,
            V2XLogLevel::ERROR,
            "BTP",
            "BTP_TX_FAIL",
            "port=" +
                std::to_string(
                    txRequest.destinationPort));

        return false;
    }

    context.MarkSuccess();

    context.reason =
        ResultCodeToString(
            confirm.result_code);

    return true;
}


//=============================================================================
// Compatibility wrapper for existing CAM callers.
//=============================================================================

bool
VanetzaBtpAdapter::SendBtpB(
    PacketContext& context,
    uint16_t destinationPort,
    const std::vector<uint8_t>& payload)
{
    BtpTxRequest request;

    request.destinationPort =
        destinationPort;

    request.destinationPortInfo = 0;

    request.transport =
        BtpTransportType::SHB;

    request.communicationProfile =
        BtpCommunicationProfile::ITS_G5;

    request.itsAid =
        vanetza::aid::CA;

    request.trafficClass = 0;

    return SendBtpB(
        context,
        request,
        payload);
}

} // namespace ns3
