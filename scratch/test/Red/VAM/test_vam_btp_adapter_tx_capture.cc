#include <vanetza/btp/header.hpp>
#include <vanetza/btp/header_conversion.hpp>

#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/common/position_fix.hpp>

#include <vanetza/dcc/data_request.hpp>
#include <vanetza/dcc/interface.hpp>

#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/pdu_conversion.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/geonet/shb_header.hpp>

#include <vanetza/net/chunk_packet.hpp>
#include <vanetza/net/osi_layer.hpp>

#include <vanetza/units/angle.hpp>
#include <vanetza/units/length.hpp>
#include <vanetza/units/time.hpp>
#include <vanetza/units/velocity.hpp>

#include <boost/variant/get.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/vam/transport/vam_transport_policy.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"

// The production adapter object references these PacketContext methods.
// This isolated harness supplies the same state transitions without
// linking the complete ns-3 application.
namespace ns3
{

void PacketContext::AdvancePipeline(PipelineStage stage)
{
    pipelineStage = stage;
    pipelineTotal = 8;
    pipelineResult = PipelineResult::BEGIN;

    switch (stage)
    {
        case PipelineStage::BTP_TX:
            pipelineStep = 4;
            break;

        case PipelineStage::GEONET_TX:
            pipelineStep = 5;
            break;

        default:
            pipelineStep = 0;
            break;
    }
}

void PacketContext::MarkSuccess()
{
    pipelineResult = PipelineResult::OK;
}

void PacketContext::MarkFailure()
{
    pipelineResult = PipelineResult::FAIL;
}

} // namespace ns3

// Logging is intentionally inert in this isolated adapter test.
void V2XWriteLog(
    const ns3::PacketContext&,
    V2XLogLevel,
    const std::string&,
    const std::string&,
    const std::string&)
{
}

namespace
{

unsigned g_failures = 0;

void Check(const char* name, bool condition)
{
    if (condition)
    {
        std::cout << "PASS " << name << '\n';
    }
    else
    {
        std::cerr << "FAIL " << name << '\n';
        ++g_failures;
    }
}

class RecordingDcc final : public vanetza::dcc::RequestInterface
{
public:
    unsigned calls{0};
    vanetza::dcc::DataRequest lastRequest{};
    std::unique_ptr<vanetza::ChunkPacket> lastPacket;

    void request(
        const vanetza::dcc::DataRequest& request,
        std::unique_ptr<vanetza::ChunkPacket> packet) override
    {
        ++calls;
        lastRequest = request;
        lastPacket = std::move(packet);
    }
};

void ConfigureMib(
    vanetza::geonet::MIB& mib,
    unsigned maximumSduSize)
{
    const vanetza::MacAddress localMac{
        0x02, 0x00, 0x00, 0x00, 0x00, 0x01
    };

    vanetza::geonet::Address localAddress(localMac);

    mib.itsGnLocalGnAddr = localAddress;
    mib.itsGnSecurity = false;
    mib.itsGnMaxSduSize = maximumSduSize;
    mib.itsGnMaxPacketLifetime.encode(
        vanetza::units::Duration::from_value(10.0));
    mib.itsGnDefaultPacketLifetime.encode(
        vanetza::units::Duration::from_value(1.0));
    mib.vanetzaDisableBeaconing = true;
}

void ConfigurePosition(
    vanetza::geonet::Router& router,
    vanetza::ManualRuntime& runtime)
{
    vanetza::PositionFix position;

    position.timestamp = runtime.now();
    position.latitude =
        vanetza::units::GeoAngle::from_value(38.7223);
    position.longitude =
        vanetza::units::GeoAngle::from_value(-9.1393);

    position.confidence.semi_major =
        vanetza::units::Length::from_value(5.0);
    position.confidence.semi_minor =
        vanetza::units::Length::from_value(5.0);
    position.confidence.orientation =
        vanetza::units::TrueNorth::from_value(0.0);

    position.course =
        vanetza::ConfidentQuantity<vanetza::units::TrueNorth>(
            vanetza::units::TrueNorth::from_value(0.0));

    position.speed =
        vanetza::ConfidentQuantity<vanetza::units::Velocity>(
            vanetza::units::Velocity::from_value(0.0));

    router.update_position(position);
}

bool RunAcceptedVamRequest()
{
    using namespace vanetza;
    using namespace vanetza::geonet;

    ManualRuntime runtime;
    MIB mib;
    ConfigureMib(mib, 1400);

    Router router(runtime, mib);
    router.set_address(mib.itsGnLocalGnAddr);
    ConfigurePosition(router, runtime);

    RecordingDcc dcc;
    router.set_access_interface(&dcc);

    ns3::VanetzaBtpAdapter adapter(router, mib);

    auto request = ns3::MakeVamBtpRequest(7);
    request.destinationPortInfo = 0xCAFE;
    request.maximumLifetimeMs = 1000;
    request.maximumHopLimit = 4;

    const std::vector<uint8_t> payload{
        0x80, 0x01, 0x02, 0x7F, 0xA5, 0x5A, 0xFE, 0x11
    };

    ns3::PacketContext context;
    context.packetId = 1001;
    context.senderStationId = 9001;

    const bool accepted =
        adapter.SendBtpB(context, request, payload);

    Check("VAM_ADAPTER_SEND_ACCEPTED", accepted);
    if (!accepted)
    {
        std::cerr << "SEND_REASON=" << context.reason << '\n';
        return false;
    }

    Check("VAM_CONTEXT_FINAL_STAGE_GEONET_TX",
        context.pipelineStage == ns3::PipelineStage::GEONET_TX);

    Check("VAM_CONTEXT_FINAL_RESULT_OK",
        context.pipelineResult == ns3::PipelineResult::OK);

    Check("VAM_DCC_REQUEST_CAPTURED_ONCE",
        dcc.calls == 1 && static_cast<bool>(dcc.lastPacket));

    if (!dcc.lastPacket)
    {
        return false;
    }

    auto& networkLayer =
        dcc.lastPacket->layer(OsiLayer::Network);

    Pdu* pdu = pdu_cast(networkLayer);

    Check("VAM_GEONET_PDU_PRESENT", pdu != nullptr);
    if (!pdu)
    {
        return false;
    }

    Check("VAM_GEONET_SECURITY_DISABLED_IN_HARNESS",
        pdu->secured() == nullptr);

    const auto& common = pdu->common();
    const auto& basic = pdu->basic();

    Check("VAM_GEONET_NEXT_HEADER_BTP_B",
        common.next_header == NextHeaderCommon::BTP_B);

    Check("VAM_GEONET_TRAFFIC_CLASS_PRESERVED",
        common.traffic_class.raw() == 7);

    Check("VAM_SHB_MAX_HOP_LIMIT_FIXED_TO_ONE",
        common.maximum_hop_limit == 1);

    Check("VAM_SHB_CURRENT_HOP_LIMIT_FIXED_TO_ONE",
        basic.hop_limit == 1);

    std::cout << "VAM_CAPTURED_SHB_MAX_HOP_LIMIT="
              << static_cast<unsigned>(common.maximum_hop_limit)
              << '\n';

    std::cout << "VAM_CAPTURED_SHB_CURRENT_HOP_LIMIT="
              << static_cast<unsigned>(basic.hop_limit)
              << '\n';

    const double lifetimeSeconds =
        basic.lifetime.decode().value();

    Check("VAM_GEONET_LIFETIME_WITHIN_1000MS",
        lifetimeSeconds > 0.0 &&
        lifetimeSeconds <= 1.05);

    auto extended = pdu->extended_variant();

    Check("VAM_GEONET_TRANSPORT_IS_SHB",
        boost::get<const ShbHeader&>(&extended) != nullptr);

    // Parse BTP-B from a copy so the captured packet remains intact.
    ChunkPacket packetCopy(*dcc.lastPacket);
    const auto btpHeader = btp::parse_btp_b(packetCopy);

    Check("VAM_BTP_DESTINATION_PORT_2018",
        btpHeader.destination_port.get() == ns3::kVamBtpPort);

    Check("VAM_BTP_DESTINATION_PORT_INFO_PRESERVED",
        btpHeader.destination_port_info.get() == 0xCAFE);

    ByteBuffer capturedPayload;
    dcc.lastPacket->layer(OsiLayer::Application).convert(
        capturedPayload);

    Check("VAM_APPLICATION_PAYLOAD_BYTE_EXACT",
        capturedPayload == payload);

    Check("VAM_DCC_PROFILE_MATCHES_GEONET_TRAFFIC_CLASS",
        dcc.lastRequest.dcc_profile ==
            map_tc_onto_profile(common.traffic_class));

    std::cout << "VAM_CAPTURED_LIFETIME_SECONDS="
              << lifetimeSeconds << '\n';

    std::cout << "VAM_CAPTURED_TRAFFIC_CLASS="
              << static_cast<unsigned>(
                     common.traffic_class.raw())
              << '\n';

    std::cout << "VAM_CAPTURED_BTP_PORT="
              << btpHeader.destination_port.get() << '\n';

    std::cout << "VAM_CAPTURED_PAYLOAD_BYTES="
              << capturedPayload.size() << '\n';

    return true;
}

bool RunRejectedVamRequest()
{
    using namespace vanetza;
    using namespace vanetza::geonet;

    ManualRuntime runtime;
    MIB mib;

    // Deliberately impossible SDU limit to exercise Router rejection.
    ConfigureMib(mib, 1);

    Router router(runtime, mib);
    router.set_address(mib.itsGnLocalGnAddr);
    ConfigurePosition(router, runtime);

    RecordingDcc dcc;
    router.set_access_interface(&dcc);

    ns3::VanetzaBtpAdapter adapter(router, mib);

    auto request = ns3::MakeVamBtpRequest(7);
    request.maximumLifetimeMs = 1000;

    const std::vector<uint8_t> payload{
        0x80, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
    };

    ns3::PacketContext context;
    context.packetId = 1002;
    context.senderStationId = 9001;

    const bool accepted =
        adapter.SendBtpB(context, request, payload);

    Check("VAM_ROUTER_REJECTION_RETURNS_FALSE", !accepted);

    Check("VAM_REJECTED_REQUEST_NOT_DELIVERED_TO_DCC",
        dcc.calls == 0 && !dcc.lastPacket);

    Check("VAM_REJECTED_CONTEXT_MARKED_FAILURE",
        context.pipelineResult == ns3::PipelineResult::FAIL);

    std::cout << "VAM_REJECTION_REASON="
              << context.reason << '\n';

    return !accepted && dcc.calls == 0;
}

} // namespace

int main()
{
    std::cout << "===== VAM REAL BTP ADAPTER TX CAPTURE =====\n";

    RunAcceptedVamRequest();
    RunRejectedVamRequest();

    if (g_failures != 0)
    {
        std::cerr << "VAM_BTP_ADAPTER_TX_CAPTURE=FAIL failures="
                  << g_failures << '\n';
        return 1;
    }

    std::cout << "VAM_BTP_ADAPTER_TX_CAPTURE=PASS\n";
    return 0;
}
