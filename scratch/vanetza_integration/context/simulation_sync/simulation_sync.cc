#include "simulation_sync.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_mobility/vehicle_mobility_updater/vehicle_mobility_updater.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_state_updater/vehicle_state_updater.h"

namespace ns3{

bool SimulationSync::m_pendingStep = false;

void SimulationSync::ProcessVehicle(const VehicleState& parsedVs){
    auto& registry = VehicleRegistry::Instance();

    // Garantir existência do veículo

    if (!registry.Exists(parsedVs.identity.vehId)){
        VehicleFactory::CreateVehicle(parsedVs.identity.vehId);
    }

    // Obter (ou criar) o VehicleState

    VehicleState* state = registry.CreateOrGet(SimulationContext::simulationStep, parsedVs.identity.vehId);

    if (!state){
        return;
    }

    // Atualizar estado proveniente do SUMO

    VehicleStateUpdater::Apply(*state, parsedVs);

    // Aplicar ao modelo de mobilidade do ns-3

    VehicleMobilityUpdater::Apply(*state);

    // Existe pelo menos uma atualização do SUMO pronta

    NotifyStepReceived();
}

void SimulationSync::NotifyStepReceived()
{
    m_pendingStep = true;
}

bool SimulationSync::HasPendingStep()
{
    return m_pendingStep;
}

void SimulationSync::ClearPendingStep()
{
    m_pendingStep = false;
}

} // namespace ns3
