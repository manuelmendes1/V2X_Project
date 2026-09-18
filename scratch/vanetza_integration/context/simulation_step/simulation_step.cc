#include "simulation_step.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/simulation_export/simulation_export.h"

namespace ns3
{

void
SimulationStep::Execute()
{
    /*
     * SimulationStep trata apenas da evolução do estado sincronizado
     * com o SUMO e da exportação desse estado.
     *
     * CamScheduler possui agora uma cadência temporal independente
     * controlada por SimulationController::SendCam().
     */

    SimulationExport::SendVehiclesToSumo();

    ++SimulationContext::simulationStep;
}

} // namespace ns3
