#include "vanetza_integration/cpm/emitter/cpm_rsu_runtime.h"
#include "vanetza_integration/cpm/emitter/cpm_rsu_position_provider.h"

#include <exception>
#include <utility>

#include <ns3/log.h>

#include <vanetza/common/bit_number.hpp>
#include <vanetza/common/clock.hpp>
#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/geonet/address.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/net/mac_address.hpp>
#include <vanetza/security/delegating_security_entity.hpp>
#include <vanetza/security/sign_service.hpp>
#include <vanetza/security/v3/sign_service.hpp>

#include "vanetza_integration/context/its_time/its_start_time_parser.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/its_station/address/its_station_address_builder.h"
#include "vanetza_integration/cpm/emitter/cpm_emitter_endpoint.h"
#include "vanetza_integration/cpm/service/cpm_service.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_dcc_adapter.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"
#include "vanetza_integration/wrappers/vanetza/security/cpm_v3_sign_service.h"
#include "vanetza_integration/wrappers/vanetza/security/v2x_v3_verify_service.h"

namespace ns3
{

CpmRsuRuntime::CpmRsuRuntime(CpmEmitterEndpoint& endpoint)
    : m_endpoint(endpoint)
{
}

CpmRsuRuntime::~CpmRsuRuntime()
{
    Shutdown();
}

bool
CpmRsuRuntime::Initialize()
{
    if (m_initialized)
    {
        return true;
    }

    const ItsStationState& station = m_endpoint.StationState();
    if (!station.networkInitialized ||
        station.node == nullptr ||
        station.socket == nullptr)
    {
        NS_LOG_UNCOND("[CPM_RSU_RUNTIME] ERROR station network is not initialized");
        return false;
    }

    if (SimulationContext::itsStartTime.empty())
    {
        NS_LOG_UNCOND("[CPM_RSU_RUNTIME] ERROR explicit --itsStartTime required");
        return false;
    }

    vanetza::Clock::time_point itsStartTime;
    std::string parseError;

    if (!ItsStartTimeParser::ParseUtc(
            SimulationContext::itsStartTime,
            itsStartTime,
            parseError))
    {
        NS_LOG_UNCOND(
            "[CPM_RSU_RUNTIME] ERROR invalid UTC ITS start time: "
            << SimulationContext::itsStartTime
            << " reason=" << parseError);
        return false;
    }

    m_runtime = std::make_unique<vanetza::ManualRuntime>(itsStartTime);
    m_position = std::make_unique<CpmRsuPositionProvider>(
        m_endpoint.LatitudeDeg(),
        m_endpoint.LongitudeDeg());

    if (SimulationContext::cpmPkiDirectory.empty())
    {
        NS_LOG_UNCOND(
            "[CPM_RSU_RUNTIME] ERROR --cpmPkiDir is required for CPM signing");
        return false;
    }

    try
    {
        auto signService =
            std::make_unique<CpmV3SignService>(
                *m_runtime,
                *m_position,
                SimulationContext::cpmPkiDirectory);

        auto verifyService =
            std::make_unique<V2xV3VerifyService>(
                *m_runtime,
                *m_position,
                SimulationContext::cpmPkiDirectory);

        m_securityEntity =
            std::make_unique<vanetza::security::DelegatingSecurityEntity>(
                std::move(signService),
                std::move(verifyService));
    }
    catch (const std::exception& error)
    {
        NS_LOG_UNCOND(
            "[CPM_RSU_RUNTIME] ERROR CPM security initialization failed: "
            << error.what());
        m_securityEntity.reset();
        return false;
    }
    m_mib = std::make_unique<vanetza::geonet::MIB>();
    m_mib->vanetzaDisableBeaconing = false;
    m_mib->itsGnSecurity = true;

    vanetza::geonet::Address localAddress;
    if (!ItsStationAddressBuilder::Build(station.identity, localAddress))
    {
        NS_LOG_UNCOND(
            "[CPM_RSU_RUNTIME] ERROR failed to construct RSU GN_ADDR");
        Shutdown();
        return false;
    }

    m_mib->itsGnLocalGnAddr = localAddress;

    m_router =
        std::make_unique<vanetza::geonet::Router>(*m_runtime, *m_mib);
    m_router->set_security_entity(m_securityEntity.get());
    m_router->set_address(localAddress);

    m_router->packet_dropped =
        [](vanetza::geonet::Router::PacketDropReason reason)
        {
            NS_LOG_UNCOND(
                "[CPM_RSU_ROUTER_DROP] reason="
                << static_cast<int>(reason));
        };

    m_router->forwarding_stopped =
        [](vanetza::geonet::Router::ForwardingStopReason reason)
        {
            NS_LOG_UNCOND(
                "[CPM_RSU_ROUTER_FORWARD_STOP] reason="
                << static_cast<int>(reason));
        };

    m_access = std::make_unique<Ns3AccessAdapter>(
        m_endpoint.StationState(), *m_mib, *m_router);

    m_dccAdapter = std::make_unique<Ns3DccAdapter>(
        *m_access,
        *m_runtime,
        itsStartTime,
        m_endpoint.LogicalId(),
        station.node->GetId(),
        m_endpoint.StationId(),
        "rsu");

    m_router->set_access_interface(m_dccAdapter.get());
    m_router->update_position(m_position->position_fix());

    m_btpAdapter =
        std::make_unique<VanetzaBtpAdapter>(*m_router, *m_mib);

    m_initialized = true;
    m_dccAdapter->SynchronizeRuntimeScheduler();

    NS_LOG_UNCOND(
        "[CPM_RSU_RUNTIME] READY"
        << " logical_id=" << m_endpoint.LogicalId()
        << " station_id=" << m_endpoint.StationId()
        << " node_id=" << station.node->GetId()
        << " ipv4=" << station.ipv4Address
        << " link_mac=" << station.linkMac
        << " signer=ETSI_V3_CPM");

    return true;
}

void
CpmRsuRuntime::Shutdown()
{
    m_btpAdapter.reset();
    m_dccAdapter.reset();
    m_access.reset();
    m_router.reset();
    m_securityEntity.reset();
    m_position.reset();
    m_mib.reset();
    m_runtime.reset();
    m_initialized = false;
}

bool
CpmRsuRuntime::IsInitialized() const
{
    return m_initialized;
}

bool
CpmRsuRuntime::Send(
    const std::shared_ptr<PacketContext>& context,
    const CpmInput& input,
    uint8_t trafficClass)
{
    if (!m_initialized || !m_btpAdapter || !m_dccAdapter)
    {
        return false;
    }

    // Refresh ITS time and the local GeoNetworking position before
    // CpmService reaches Router::request() and constructs the SHB.
    m_dccAdapter->SynchronizeRuntimeForTransmit();
    m_router->update_position(m_position->position_fix());

    const CpmTransportGateway gateway(
        [this](
            PacketContext& packetContext,
            const BtpTxRequest& request,
            const std::vector<uint8_t>& payload)
        {
            const bool sent =
                m_btpAdapter->SendBtpB(
                    packetContext, request, payload);

            m_dccAdapter->SynchronizeRuntimeScheduler();
            return sent;
        });

    return CpmService::Send(
        context, input, gateway, trafficClass);
}

} // namespace ns3
