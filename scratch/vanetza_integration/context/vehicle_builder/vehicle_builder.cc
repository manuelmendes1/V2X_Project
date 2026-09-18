#include "vehicle_builder.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_identity/vehicle_identity_initializer.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"

namespace ns3 {

    VehicleState* VehicleBuilder::Create(const std::string& vehId, Ptr<Node> node, Ptr<Socket> socket) {

        VehicleState* state = InitializeVehicleState(vehId, node, socket);

        if (!state){
            return nullptr;
        }

        //----------------------------------------------------------------------
        // STEP 1: authoritative vehicle identity
        //
        // VehicleIdentityInitializer is the sole authority for:
        //   - nodeId
        //   - stationId
        //
        // vehId originates from SUMO / VehicleBuilder.
        //
        // Identity MUST be complete before VanetzaRuntime is created.
        //----------------------------------------------------------------------
        VehicleIdentityInitializer::Initialize(*state);

        if (state->identity.stationId == 0) {
            NS_LOG_UNCOND("[VEHICLE_BUILDER] ERROR: invalid stationId=0 for veh=" << state->identity.vehId);
            return nullptr;
        }

        //----------------------------------------------------------------------
        // STEP 2: security identity/context
        //----------------------------------------------------------------------
        VehicleSecurityInitializer::Initialize(*state);

        //----------------------------------------------------------------------
        // STEP 3: Vanetza Runtime
        //
        // At this point VehicleIdentity is definitive.
        //
        // Therefore VanetzaRuntime::Initialize() receives:
        //   vehId     = authoritative VehicleIdentity.vehId
        //   nodeId    = authoritative VehicleIdentity.nodeId
        //   stationId = authoritative VehicleIdentity.stationId
        //
        // GN_ADDR is materialized from this definitive stationId.
        //----------------------------------------------------------------------
        if (!state->runtime) {

            state->runtime = std::make_unique<VanetzaRuntime>(*state);

            if (!state->runtime->Initialize()){

                state->runtime.reset();
                return nullptr;
            }
        }

        LogVehicleCreation(*state);

        return state;
    }

    VehicleState* VehicleBuilder::InitializeVehicleState(const std::string& vehId, Ptr<Node> node, Ptr<Socket> socket) {

        auto& registry = VehicleRegistry::Instance();

        VehicleState* state = registry.CreateOrGet(SimulationContext::simulationStep, vehId);

        if (!state){
            return nullptr;
        }

        state->node = node;
        state->socket = socket;
        state->identity.vehId = vehId;

        //----------------------------------------------------------------------
        // IMPORTANT:
        //
        // Do NOT create VanetzaRuntime here.
        //
        // VehicleIdentityInitializer::Initialize() must run first so that
        // nodeId and stationId are authoritative before VanetzaRuntime
        // materializes GN_ADDR and creates the Router.
        //----------------------------------------------------------------------

        return state;
    }


void
VehicleBuilder::LogVehicleCreation(
    const VehicleState& state)
{
    V2XWriteLog(
        SimulationContext::simulationStep,
        V2XLogLevel::INFO,
        "VEHICLE",
        "IDENTITY_READY",
        state.identity.vehId,
        state.identity.nodeId,
        state.identity.stationId,
        -1,
        "Vehicle identity initialized before VanetzaRuntime");
}

} // namespace ns3
