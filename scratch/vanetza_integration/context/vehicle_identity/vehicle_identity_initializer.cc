#include "vehicle_identity_initializer.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"

#include "ns3/log.h"

#include <string>

namespace ns3
{

void
VehicleIdentityInitializer::Initialize(VehicleState& state)
{
    //======================================================================
    // Preconditions
    //======================================================================

    NS_ABORT_MSG_IF(
        !state.node,
        "VehicleIdentityInitializer requires a valid ns-3 Node");

    NS_ABORT_MSG_IF(
        state.identity.vehId.empty(),
        "VehicleIdentityInitializer requires a valid SUMO vehicle ID");

    //======================================================================
    // AUTHORITATIVE VEHICLE IDENTITY
    //
    // VehicleIdentityInitializer is the single authority responsible for
    // initializing the logical identity of a vehicle.
    //
    // Generated/assigned here:
    //   - nodeId
    //   - stationId
    //
    // The VehicleRegistry only indexes these values and MUST NOT modify
    // VehicleState::identity.
    //======================================================================

    state.identity.nodeId =
        state.node->GetId();

    //---------------------------------------------------------------------
    // stationId
    //
    // 0 is reserved as "not initialized".
    //
    // stationId is derived deterministically from the unique ns-3 nodeId.
    // This guarantees uniqueness for the simulated vehicle population,
    // provided the nodeId remains unique.
    //
    // For the current simulation, nodeId=0..199 becomes
    // stationId=1..200.
    //
    // GN_ADDR.MID is materialized downstream from this authoritative
    // stationId by VanetzaGeoNetAdapter.
    //---------------------------------------------------------------------

    const ItsStationIdentityOwner stationOwner{
        ItsStationIdentityOwnerKind::Vehicle,
        state.identity.vehId
    };

    auto& identityAuthority =
        ItsStationIdentityAuthority::Instance();

    uint32_t stationId = 0;

    // Preserve the logical vehicle identity across SUMO re-entry.
    // The first accepted presence derives its ID from nodeId + 1.
    const auto existingStationId =
        identityAuthority.FindStationId(stationOwner);

    if (existingStationId.has_value())
    {
        stationId = *existingStationId;
    }
    else
    {
        NS_ABORT_MSG_IF(
            !identityAuthority.ReserveNodeDerived(
                stationOwner,
                state.identity.nodeId,
                stationId),
            "[VEHICLE_IDENTITY] global stationId reservation failed"
                << " vehicle=" << state.identity.vehId
                << " node_id=" << state.identity.nodeId);
    }

    state.identity.stationId = stationId;

    NS_ABORT_MSG_IF(
        state.identity.stationId == 0,
        "VehicleIdentityInitializer generated invalid stationId=0");

    //----------------------------------------------------------------------
    // Validate node identity.
    //----------------------------------------------------------------------

    NS_ABORT_MSG_IF(
        state.identity.nodeId != state.node->GetId(),
        "VehicleIdentityInitializer generated inconsistent nodeId");

    //======================================================================
    // Register reverse indexes
    //
    // VehicleRegistry is an index service only.
    //======================================================================

    auto& registry =
        VehicleRegistry::Instance();

    registry.RegisterNodeId(
        SimulationContext::simulationStep,
        state.identity.vehId,
        state.identity.nodeId);

    registry.RegisterStationId(
        SimulationContext::simulationStep,
        state.identity.vehId,
        state.identity.stationId);
}

} // namespace ns3
