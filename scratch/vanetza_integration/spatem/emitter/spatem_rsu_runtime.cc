#include "spatem_rsu_runtime.h"

#include "vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h"
#include "vanetza_integration/wrappers/vanetza/access/ns3_dcc_adapter.h"
#include "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.h"


#include <utility>

#include <ns3/log.h>

#include <vanetza/common/clock.hpp>
#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/geonet/address.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/security/delegating_security_entity.hpp>
#include <vanetza/security/sign_service.hpp>
#include <vanetza/security/v3/sign_service.hpp>

#include "vanetza_integration/context/its_time/its_start_time_parser.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/spatem/emitter/its_station_address_builder.h"
#include "vanetza_integration/spatem/emitter/spatem_emitter_endpoint.h"
#include "vanetza_integration/spatem/emitter/spatem_rsu_position_provider.h"
#include "vanetza_integration/spatem/service/spatem_service.h"
#include "vanetza_integration/spatem/transport/spatem_transport_gateway.h"
#include "vanetza_integration/wrappers/vanetza/security/denm_v3_verify_service.h"

namespace ns3
{

SpatemRsuRuntime::SpatemRsuRuntime(SpatemEmitterEndpoint& endpoint)
    : m_endpoint(endpoint)
{
}

SpatemRsuRuntime::~SpatemRsuRuntime()
{
    Shutdown();
}

bool
SpatemRsuRuntime::Initialize()
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
        NS_LOG_UNCOND(
            "[SPATEM_RSU_RUNTIME] ERROR: station network is not initialized");
        return false;
    }

    if (SimulationContext::itsStartTime.empty())
    {
        NS_LOG_UNCOND(
            "[SPATEM_RSU_RUNTIME] ERROR: explicit --itsStartTime is required");
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
            "[SPATEM_RSU_RUNTIME] ERROR: invalid UTC ITS start time: "
            << SimulationContext::itsStartTime
            << " reason=" << parseError);
        return false;
    }

    m_runtime =
        std::make_unique<vanetza::ManualRuntime>(itsStartTime);

    m_position =
        std::make_unique<SpatemRsuPositionProvider>(
            m_endpoint.LatitudeDeg(),
            m_endpoint.LongitudeDeg());

    // Match the existing GeoNetworking runtime's dummy signer policy.
    // This provides a transmission path, not proof of production PKI signing.
    auto signService =
        std::make_unique<vanetza::security::v3::DummySignService>(
            *m_runtime);

    auto verifyService =
        std::make_unique<DenmV3VerifyService>(*m_runtime);

    m_securityEntity =
        std::make_unique<vanetza::security::DelegatingSecurityEntity>(
            std::move(signService),
            std::move(verifyService));

    m_mib = std::make_unique<vanetza::geonet::MIB>();
    m_mib->vanetzaDisableBeaconing = false;
    m_mib->itsGnSecurity = true;

    vanetza::geonet::Address localAddress;

    if (!ItsStationAddressBuilder::Build(station, localAddress))
    {
        NS_LOG_UNCOND(
            "[SPATEM_RSU_RUNTIME] ERROR: failed to construct RSU GN_ADDR");
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
                "[SPATEM_RSU_ROUTER_DROP] reason="
                << static_cast<int>(reason));
        };

    m_router->forwarding_stopped =
        [](vanetza::geonet::Router::ForwardingStopReason reason)
        {
            NS_LOG_UNCOND(
                "[SPATEM_RSU_ROUTER_FORWARD_STOP] reason="
                << static_cast<int>(reason));
        };

    m_access =
        std::make_unique<Ns3AccessAdapter>(
            m_endpoint.StationState(),
            *m_mib,
            *m_router);

    m_dccAdapter =
        std::make_unique<Ns3DccAdapter>(
            *m_access,
            *m_runtime,
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
        "[SPATEM_RSU_RUNTIME] READY"
        << " logical_id=" << m_endpoint.LogicalId()
        << " station_id=" << m_endpoint.StationId()
        << " node_id=" << station.node->GetId()
        << " signer=DUMMY");

    return true;
}

void
SpatemRsuRuntime::Shutdown()
{
    // Preserve dependency order: consumers first, then Router/security,
    // then providers and the ManualRuntime they reference.
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
SpatemRsuRuntime::IsInitialized() const
{
    return m_initialized;
}

bool
SpatemRsuRuntime::Send(
    const std::shared_ptr<PacketContext>& context,
    const SpatemInput& input,
    const std::string& reason)
{
    if (!m_initialized || !m_btpAdapter || !m_dccAdapter)
    {
        return false;
    }

    const SpatemTransportGateway gateway(
        [this](
            PacketContext& packetContext,
            const BtpTxRequest& request,
            const std::vector<uint8_t>& payload)
        {
            const bool sent =
                m_btpAdapter->SendBtpB(
                    packetContext,
                    request,
                    payload);

            m_dccAdapter->SynchronizeRuntimeScheduler();
            return sent;
        });

    return SpatemService::Send(
        context,
        input,
        gateway,
        reason);
}

} // namespace ns3
