#include "vanetza_geonet_adapter.h"

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/file_credential_material_resolver.h"
#include "vanetza_integration/context/rx_bridge/rx_bridge_context.h"
#include "vanetza_integration/wrappers/vanetza/geonet_mid_adapter.h"
#include "vanetza_integration/wrappers/vanetza/position/ns3_position_provider.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_dcc_adapter.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_sign_service.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_verify_service.h"

#include <vanetza/common/bit_number.hpp>
#include <vanetza/common/byte_buffer.hpp>
#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/geonet/address.hpp>
#include <vanetza/geonet/basic_header.hpp>
#include <vanetza/geonet/common_header.hpp>
#include <vanetza/geonet/header_type.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/parser.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/net/mac_address.hpp>
#include <vanetza/net/packet_variant.hpp>
#include <vanetza/security/certificate_validity.hpp>
#include <vanetza/security/delegating_security_entity.hpp>
#include <vanetza/security/sign_service.hpp>
#include <vanetza/security/verify_service.hpp>
#include <vanetza/security/v3/sign_service.hpp>
#include <vanetza/security/secured_message.hpp>

#include <ns3/log.h>
#include <ns3/simulator.h>

#include "vanetza_integration/context/simulation_context/simulation_context.h"

#include <chrono>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace ns3 {

    struct VanetzaGeoNetAdapter::Impl {

        VehicleState& vehicle;

        std::unique_ptr<vanetza::ManualRuntime> runtime;
        vanetza::Clock::time_point itsStartTime{};
        std::unique_ptr<vanetza::security::SecurityEntity> securityEntity;

        // Non-owning pointer to the real DENM signer. Ownership remains with
        // DelegatingSecurityEntity after the SignService unique_ptr is moved.
        DenmV3SignService* denmSigner = nullptr;

        // Credential material resolution is deliberately owned outside the
        // DENM signer so filesystem access remains outside the signer layer.
        FileCredentialMaterialResolver denmCredentialMaterialResolver;

        std::unique_ptr<vanetza::geonet::MIB> mib;
        std::unique_ptr<vanetza::geonet::Router> router;

        std::unique_ptr<Ns3AccessAdapter> access;
        std::unique_ptr<Ns3DccAdapter> dccAdapter;
        std::unique_ptr<Ns3PositionProvider> position;
        std::unique_ptr<VanetzaBtpAdapter> btpAdapter;

        GeoNetMid localMid{};

        bool initialized = false;

        explicit Impl(VehicleState& v) : vehicle(v) {}
    };

    VanetzaGeoNetAdapter::VanetzaGeoNetAdapter(VehicleState& vehicle) : m_impl(std::make_unique<Impl>(vehicle)) {}

    VanetzaGeoNetAdapter::~VanetzaGeoNetAdapter() {
        Shutdown();
    }

    bool VanetzaGeoNetAdapter::Initialize() {

        if (m_impl->initialized){
            return true;
        }

        //=====================================================================
        // Manual runtime
        //
        // The Vanetza clock is an absolute ITS time domain. ns-3 time is
        // relative to simulation start, therefore the configured ITS start
        // time must be the runtime epoch.
        //=====================================================================

        const bool hasRealDenmCredential =
            !m_impl->vehicle.security.certFile.empty() &&
            !m_impl->vehicle.security.keyFile.empty();

        if (hasRealDenmCredential &&
            SimulationContext::itsStartTime.empty())
        {
            NS_LOG_UNCOND(
                "[SECURITY] DENM V3 signer requires "
                "--itsStartTime to enable certificate time validation");
            return false;
        }

        if (!SimulationContext::itsStartTime.empty())
        {
            try
            {
                std::string normalizedItsStartTime = SimulationContext::itsStartTime;

                const auto separator = normalizedItsStartTime.find('T');
                if (separator != std::string::npos)
                {
                    normalizedItsStartTime[separator] = ' ';
                }

                try
                {
                    m_impl->itsStartTime =
                        vanetza::Clock::at(normalizedItsStartTime);
                }
                catch (...)
                {
                    NS_LOG_UNCOND(
                        "[SECURITY] invalid ITS start time: "
                        << SimulationContext::itsStartTime);
                    return false;
                }
            }
            catch (...)
            {
                NS_LOG_UNCOND(
                    "[SECURITY] invalid ITS start time: "
                    << SimulationContext::itsStartTime);
                return false;
            }
        }

        m_impl->runtime =
            std::make_unique<vanetza::ManualRuntime>(
                m_impl->itsStartTime);

        //=====================================================================
        // Position provider
        //
        // Must exist before the real V3 DENM signer because
        // DefaultSignHeaderPolicy uses it to populate generationLocation.
        //=====================================================================

        m_impl->position = std::make_unique<Ns3PositionProvider>(m_impl->vehicle);

        //=====================================================================
        // GeoNetworking security
        //
        // DENM / AID 37 -> real ETSI ITS Security V3 signing
        // CAM  / other  -> previous dummy signer
        //=====================================================================

        std::unique_ptr<vanetza::security::SignService> signService;

        if (hasRealDenmCredential)
        {
            const auto& security =
                m_impl->vehicle.security;

            const VehicleCredential* activeCredential =
                security.credentialPool.GetActiveCredential();

            if (activeCredential == nullptr)
            {
                NS_LOG_UNCOND(
                    "[SECURITY] DENM V3 signer unavailable "
                    "due to missing active credential");
                return false;
            }

            auto realDenmSigner =
                std::make_unique<DenmV3SignService>(
                    *m_impl->runtime,
                    *m_impl->position,
                    security.credentialPool,
                    m_impl->denmCredentialMaterialResolver);

            // Keep a non-owning reference before transferring ownership to
            // DelegatingSecurityEntity.
            m_impl->denmSigner =
                realDenmSigner.get();

            signService =
                std::move(realDenmSigner);

            NS_LOG_UNCOND(
                "[SECURITY] DENM V3 signer=REAL "
                << "veh="
                << m_impl->vehicle.identity.vehId
                << " cert="
                << m_impl->vehicle.security.certFile);
        }
        else
        {
            m_impl->denmSigner = nullptr;

            signService =
                std::make_unique<
                    vanetza::security::v3::DummySignService>(
                    *m_impl->runtime);

            NS_LOG_UNCOND(
                "[SECURITY] DENM V3 signer=DUMMY "
                << "veh="
                << m_impl->vehicle.identity.vehId);
        }

        auto verifyService =
            std::make_unique<DenmV3VerifyService>(
                *m_impl->runtime);

        NS_LOG_UNCOND(
            "[SECURITY] DENM V3 verifier=REAL "
            << "veh=" << m_impl->vehicle.identity.vehId);

        m_impl->securityEntity =
            std::make_unique<
                vanetza::security::DelegatingSecurityEntity>(
                std::move(signService),
                std::move(verifyService));
        
        //=====================================================================
        // MIB
        //=====================================================================

        m_impl->mib = std::make_unique<vanetza::geonet::MIB>();

        m_impl->mib->vanetzaDisableBeaconing = false;

        m_impl->mib->itsGnSecurity = true;

        //=====================================================================
        // GN_ADDR
        //=====================================================================

        vanetza::MacAddress mac = vanetza::create_mac_address(m_impl->vehicle.identity.stationId);

        mac.octets[0] = 0x02;
        mac.octets[1] = 0x00;

        vanetza::geonet::Address gnAddr;

        gnAddr.mid(mac);

        gnAddr.station_type(vanetza::geonet::StationType::Passenger_Car);

        gnAddr.is_manually_configured(true);

        gnAddr.country_code(vanetza::BitNumber<unsigned, 10>(0));

        m_impl->mib->itsGnLocalGnAddr = gnAddr;

        //=====================================================================
        // Local MID -> neutral context representation
        //=====================================================================

        m_impl->localMid = VanetzaGeoNetMidAdapter::ToGeoNetMid(gnAddr.mid());

        VehicleRegistry::Instance().RegisterMid(m_impl->localMid, m_impl->vehicle);

        //=====================================================================
        // Router
        //=====================================================================

        m_impl->router = std::make_unique<vanetza::geonet::Router>(*m_impl->runtime, *m_impl->mib);

        m_impl->router->set_security_entity(m_impl->securityEntity.get());

        m_impl->router->set_address(gnAddr);

        //=====================================================================
        // Diagnostics
        //=====================================================================

        m_impl->router->packet_dropped = [](vanetza::geonet::Router::PacketDropReason reason){
            NS_LOG_UNCOND("[ROUTER_DROP] reason=" << static_cast<int>(reason));
        };

        m_impl->router->forwarding_stopped = [](vanetza::geonet::Router::ForwardingStopReason reason){
            NS_LOG_UNCOND("[ROUTER_FORWARD_STOP] reason=" << static_cast<int>(reason));
        };

        //=====================================================================
        // Access / DCC
        //
        // Still instantiated here temporarily, because these wrappers
        // currently require direct Router/MIB references.
        //=====================================================================

        m_impl->access = std::make_unique<Ns3AccessAdapter>(m_impl->vehicle, *m_impl->mib, *m_impl->router);

        m_impl->dccAdapter = std::make_unique<Ns3DccAdapter>(
            *m_impl->access,
            *m_impl->runtime,
            m_impl->vehicle.identity.vehId,
            m_impl->vehicle.identity.nodeId,
            m_impl->vehicle.identity.stationId);

        m_impl->router->set_access_interface(m_impl->dccAdapter.get());

        //=====================================================================
        // Position provider
        //=====================================================================

        //m_impl->position = std::make_unique<Ns3PositionProvider>(m_impl->vehicle);

        m_impl->router->update_position(m_impl->position->position_fix());

        //=====================================================================
        // BTP
        //=====================================================================

        m_impl->btpAdapter = std::make_unique<VanetzaBtpAdapter>(*m_impl->router, *m_impl->mib);

        m_impl->initialized = true;

        return true;
    }

    void VanetzaGeoNetAdapter::Shutdown() {

        if (!m_impl || !m_impl->initialized) {
            return;
        }

        // Preserve the existing destruction dependency order.
        m_impl->btpAdapter.reset();
        m_impl->dccAdapter.reset();
        m_impl->access.reset();
        m_impl->router.reset();
        m_impl->securityEntity.reset();

        // DelegatingSecurityEntity owns the real DENM signer.
        // The cached pointer is therefore invalid after securityEntity.reset().
        m_impl->denmSigner = nullptr;

        m_impl->position.reset();
        m_impl->mib.reset();
        m_impl->runtime.reset();

        m_impl->initialized = false;
    }

    bool VanetzaGeoNetAdapter::IsInitialized() const {
        return m_impl && m_impl->initialized;
    }

    bool VanetzaGeoNetAdapter::UpdatePosition() {
        if (!IsInitialized()) {
            return false;
        }

        m_impl->router->update_position(m_impl->position->position_fix());

        return true;
    }


std::string
VanetzaGeoNetAdapter::ClassifyRxGeoNetworking(
    const std::vector<uint8_t>& buffer) const
{
    if (buffer.empty())
    {
        return "EMPTY";
    }

    vanetza::ByteBuffer bytes(
        buffer.begin(),
        buffer.end());

    //---------------------------------------------------------------------
    // Parse Basic Header.
    //---------------------------------------------------------------------

    vanetza::geonet::Parser parser(
        bytes.begin(),
        bytes.end());

    vanetza::geonet::BasicHeader basic;

    if (parser.parse_basic(basic) == 0)
    {
        return "INVALID_BASIC_HEADER";
    }

    //---------------------------------------------------------------------
    // Unsecured GeoNetworking packet.
    //---------------------------------------------------------------------

    if (basic.next_header ==
        vanetza::geonet::NextHeaderBasic::Common)
    {
        vanetza::geonet::CommonHeader common;

        if (parser.parse_common(common) == 0)
        {
            return "TRUNCATED_COMMON_HEADER";
        }

        switch (common.header_type)
        {
            case vanetza::geonet::HeaderType::Beacon:
                return "BEACON";

            case vanetza::geonet::HeaderType::GeoUnicast:
                return "GEO_UNICAST";

            case vanetza::geonet::HeaderType::GeoAnycast_Circle:
                return "GEO_ANYCAST_CIRCLE";

            case vanetza::geonet::HeaderType::GeoAnycast_Rect:
                return "GEO_ANYCAST_RECT";

            case vanetza::geonet::HeaderType::GeoAnycast_Elip:
                return "GEO_ANYCAST_ELLIPSE";

            case vanetza::geonet::HeaderType::GeoBroadcast_Circle:
                return "GEO_BROADCAST_CIRCLE";

            case vanetza::geonet::HeaderType::GeoBroadcast_Rect:
                return "GEO_BROADCAST_RECT";

            case vanetza::geonet::HeaderType::GeoBroadcast_Elip:
                return "GEO_BROADCAST_ELLIPSE";

            case vanetza::geonet::HeaderType::TSB_Single_Hop:
                return "TSB_SINGLE_HOP";

            case vanetza::geonet::HeaderType::TSB_Multi_Hop:
                return "TSB_MULTI_HOP";

            case vanetza::geonet::HeaderType::LS_Request:
                return "LS_REQUEST";

            case vanetza::geonet::HeaderType::LS_Reply:
                return "LS_REPLY";

            default:
                return "UNKNOWN_COMMON_HEADER";
        }
    }

    //---------------------------------------------------------------------
    // Secured GeoNetworking packet.
    //
    // Do NOT use fixed offsets. The SecuredMessage is ASN.1/OER and its
    // encoded size is variable.
    //---------------------------------------------------------------------

    if (basic.next_header ==
        vanetza::geonet::NextHeaderBasic::Secured)
    {
        vanetza::security::SecuredMessage secured;

        if (parser.parse_secured(secured) == 0)
        {
            return "INVALID_SECURED_MESSAGE";
        }

        //-----------------------------------------------------------------
        // Normalize V2/V3 secured payload into a ByteBuffer.
        //-----------------------------------------------------------------

        vanetza::ByteBuffer payloadBytes;

        boost::apply_visitor(
            [&payloadBytes](const auto& securedMessage)
            {
                using Message =
                    std::decay_t<decltype(securedMessage)>;

                if constexpr (
                    std::is_same_v<
                        Message,
                        vanetza::security::v3::SecuredMessage>)
                {
                    const vanetza::PacketVariant payload =
                        securedMessage.payload();

                    boost::apply_visitor(
                        [&payloadBytes](const auto& packet)
                        {
                            using Packet =
                                std::decay_t<decltype(packet)>;

                            if constexpr (
                                std::is_same_v<
                                    Packet,
                                    vanetza::CohesivePacket>)
                            {
                                const auto range =
                                    packet[
                                        vanetza::OsiLayer::Network];

                                payloadBytes.assign(
                                    range.begin(),
                                    range.end());
                            }
                            else
                            {
                                packet[
                                    vanetza::OsiLayer::Network]
                                    .convert(payloadBytes);
                            }
                        },
                        payload);
                }
                else
                {
                    //-----------------------------------------------------------------
                    // V2 SecuredMessage stores the payload as a member.
                    //-----------------------------------------------------------------

                    const vanetza::security::v2::Payload& payload =
                        securedMessage.payload;

                    boost::apply_visitor(
                        [&payloadBytes](const auto& packet)
                        {
                            using Packet =
                                std::decay_t<decltype(packet)>;

                            if constexpr (
                                std::is_same_v<
                                    Packet,
                                    vanetza::CohesivePacket>)
                            {
                                const auto range =
                                    packet[
                                        vanetza::OsiLayer::Network];

                                payloadBytes.assign(
                                    range.begin(),
                                    range.end());
                            }
                            else
                            {
                                packet[
                                    vanetza::OsiLayer::Network]
                                    .convert(payloadBytes);
                            }
                        },
                        payload.data);
                }
            },
            secured);

        if (payloadBytes.empty())
        {
            return "SECURED_PACKET_NO_PAYLOAD";
        }

        //-----------------------------------------------------------------
        // Parse the actual Common Header contained in the secured payload.
        //-----------------------------------------------------------------

        vanetza::geonet::Parser payloadParser(
            payloadBytes.begin(),
            payloadBytes.end());

        vanetza::geonet::CommonHeader common;

        if (payloadParser.parse_common(common) == 0)
        {
            return "SECURED_PACKET_INVALID_PAYLOAD";
        }

        switch (common.header_type)
        {
            case vanetza::geonet::HeaderType::Beacon:
                return "BEACON";

            case vanetza::geonet::HeaderType::GeoUnicast:
                return "GEO_UNICAST";

            case vanetza::geonet::HeaderType::GeoAnycast_Circle:
                return "GEO_ANYCAST_CIRCLE";

            case vanetza::geonet::HeaderType::GeoAnycast_Rect:
                return "GEO_ANYCAST_RECT";

            case vanetza::geonet::HeaderType::GeoAnycast_Elip:
                return "GEO_ANYCAST_ELLIPSE";

            case vanetza::geonet::HeaderType::GeoBroadcast_Circle:
                return "GEO_BROADCAST_CIRCLE";

            case vanetza::geonet::HeaderType::GeoBroadcast_Rect:
                return "GEO_BROADCAST_RECT";

            case vanetza::geonet::HeaderType::GeoBroadcast_Elip:
                return "GEO_BROADCAST_ELLIPSE";

            case vanetza::geonet::HeaderType::TSB_Single_Hop:
                return "TSB_SINGLE_HOP";

            case vanetza::geonet::HeaderType::TSB_Multi_Hop:
                return "TSB_MULTI_HOP";

            case vanetza::geonet::HeaderType::LS_Request:
                return "LS_REQUEST";

            case vanetza::geonet::HeaderType::LS_Reply:
                return "LS_REPLY";

            default:
                return "SECURED_PACKET";
        }
    }

    return "UNKNOWN_BASIC_NEXT_HEADER";
}


bool
VanetzaGeoNetAdapter::Receive(
    PacketContext& context,
    const std::vector<uint8_t>& buffer,
    const Mac48Address& senderMac)
{
    if (!IsInitialized() ||
        !m_impl->runtime ||
        !m_impl->router)
    {
        NS_LOG_UNCOND(
            "[GEO_RX] ERROR: GeoNetworking adapter not initialized");

        return false;
    }

    if (buffer.empty())
    {
        NS_LOG_UNCOND(
            "[GEO_RX] ERROR: empty GeoNetworking frame");

        return false;
    }

    try
    {
        if (!SynchronizeTimeAndPosition())
        {
            NS_LOG_UNCOND(
                "[GEO_RX] ERROR: failed to synchronize "
                "Vanetza time and position");

            return false;
        }

        //=====================================================================
        // Convert the received ns-3 frame into Vanetza's network packet.
        //=====================================================================

        vanetza::ByteBuffer bytes(
            buffer.begin(),
            buffer.end());

        auto packet =
            std::make_unique<vanetza::UpPacket>(
                vanetza::CohesivePacket(
                    std::move(bytes),
                    vanetza::OsiLayer::Network));

        //=====================================================================
        // Convert the ns-3 Link MAC into Vanetza's link-layer MAC.
        //=====================================================================

        uint8_t macBytes[vanetza::MacAddress::length_bytes] = {};
        senderMac.CopyTo(macBytes);

        vanetza::MacAddress sender;
        for (std::size_t i = 0;
             i < vanetza::MacAddress::length_bytes;
             ++i)
        {
            sender.octets[i] = macBytes[i];
        }

        //=====================================================================
        // Preserve the single PacketContext through Router -> BTP.
        //
        // The BTP indication handler retrieves this exact context through
        // RxBridgeContext::Current().
        //=====================================================================

        RxBridgeContext::Push(&context);

        try
        {
            m_impl->router->indicate(
                std::move(packet),
                sender,
                vanetza::cBroadcastMacAddress);

            // Router::indicate() may have created internal ManualRuntime
            // callbacks, e.g. a CBF forwarding timer. Export the earliest
            // Runtime deadline to the ns-3 Simulator immediately after the
            // Router operation so it cannot remain dormant until a later
            // application TX calls PrepareTx().
            if (m_impl->dccAdapter)
            {
                m_impl->dccAdapter->SynchronizeRuntimeScheduler();
            }
        }
        catch (...)
        {
            RxBridgeContext::Pop();
            throw;
        }

        RxBridgeContext::Pop();

        NS_LOG_UNCOND(
            "[GEO_RX] Router indicate accepted frame"
            << " bytes="
            << buffer.size()
            << " sender="
            << sender);

        return true;
    }
    catch (const std::exception& e)
    {
        RxBridgeContext::Pop();

        NS_LOG_UNCOND(
            "[GEO_RX] ERROR: Router receive failed: "
            << e.what());

        return false;
    }
    catch (...)
    {
        RxBridgeContext::Pop();

        NS_LOG_UNCOND(
            "[GEO_RX] ERROR: Router receive failed with unknown exception");

        return false;
    }
}

bool VanetzaGeoNetAdapter::SendBtpB(
    PacketContext& context,
    const BtpTxRequest& request,
    const std::vector<uint8_t>& payload)
{
    return m_impl->btpAdapter->SendBtpB(
        context,
        request,
        payload);
}

bool VanetzaGeoNetAdapter::SendBtpB(
    PacketContext& context,
    uint16_t destinationPort,
    const std::vector<uint8_t>& payload)
{
    return m_impl->btpAdapter->SendBtpB(
        context,
        destinationPort,
        payload);
}

void
VanetzaGeoNetAdapter::SetBtpRxHandler(
    uint16_t destinationPort,
    BtpRxHandler handler)
{
    if (!IsInitialized() ||
        !m_impl->btpAdapter)
    {
        NS_LOG_UNCOND(
            "[GEO_RX] ERROR: BTP adapter not initialized");

        return;
    }

    if (!handler)
    {
        NS_LOG_UNCOND(
            "[GEO_RX] ERROR: null BTP RX handler for port="
            << destinationPort);

        return;
    }

    m_impl->btpAdapter->SetRxHandler(
        destinationPort,
        [handler = std::move(handler)](
            const VanetzaBtpAdapter::RxData& rx)
        {
            BtpRxData data;

            data.destinationPort =
                rx.destinationPort;

            data.payload =
                rx.payload;

            data.hasSourcePosition =
                rx.hasSourcePosition;

            data.latitude =
                rx.latitude;

            data.longitude =
                rx.longitude;

            data.hasTrafficClass =
                rx.hasTrafficClass;

            data.trafficClass =
                rx.trafficClass;

            data.hasItsAid =
                rx.hasItsAid;

            data.itsAid =
                rx.itsAid;

            data.hasPermissions =
                rx.hasPermissions;

            data.permissionsSize =
                rx.permissionsSize;

            data.hasLifetime =
                rx.hasLifetime;

            data.lifetimeMs =
                rx.lifetimeMs;

            data.hasSourceMid =
                rx.hasSourceMid;

            data.sourceMid =
                rx.sourceMid;

            handler(data);
        });

    NS_LOG_UNCOND(
        "[GEO_RX] BTP RX handler forwarded to adapter"
        << " port="
        << destinationPort);
}

const GeoNetMid&
VanetzaGeoNetAdapter::LocalMid() const
{
    return m_impl->localMid;
}

bool
VanetzaGeoNetAdapter::ReloadDenmCredential()
{
    if (m_impl->denmSigner == nullptr)
    {
        return false;
    }

    return m_impl->denmSigner->ReloadActiveCredential();
}


bool
VanetzaGeoNetAdapter::SynchronizeTimeAndPosition()
{
    if (!IsInitialized() ||
        !m_impl->runtime ||
        !m_impl->router ||
        !m_impl->position)
    {
        return false;
    }

    const auto simulationOffset =
        std::chrono::microseconds(
            static_cast<vanetza::Clock::rep>(
                Simulator::Now().GetMicroSeconds()));

    const auto now =
        m_impl->itsStartTime + simulationOffset;

    if (now > m_impl->runtime->now())
    {
        m_impl->runtime->trigger(now);
    }

    m_impl->router->update_position(
        m_impl->position->position_fix());

    return true;
}

} // namespace ns3
