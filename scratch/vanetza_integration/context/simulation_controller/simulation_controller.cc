#include "simulation_controller.h"

#include "vanetza_integration/cam/scheduler/cam_scheduler.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/simulation_step/simulation_step.h"
#include "vanetza_integration/context/simulation_sync/simulation_sync.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"

#include "ns3/simulator.h"

namespace ns3
{

Time SimulationController::m_stepLength = MilliSeconds(100);

// ============================================================================
// Simulation lifecycle
// ============================================================================

void
SimulationController::Start(Time stepLength)
{
    m_stepLength = stepLength;

    // Polling SUMO mantém a sua cadência original.
    Simulator::ScheduleNow(
        &SimulationController::PollLoop);

    // CAM passa a ter um scheduler independente.
    Simulator::ScheduleNow(
        &SimulationController::SendCam);
}

void
SimulationController::StopSimulation()
{
    Simulator::Stop();
}

// ============================================================================
// Main polling loop
// ============================================================================

void
SimulationController::PollLoop()
{
    SumoAdapter::Poll();

    SimulationStep();

    Simulator::Schedule(
        m_stepLength,
        &SimulationController::PollLoop);
}

// ============================================================================
// Independent CAM scheduler
// ============================================================================

void SimulationController::SendCam() {

    CamScheduler::Run();

    Simulator::Schedule(CamScheduler::RunInterval(), &SimulationController::SendCam);
}

// ============================================================================
// Simulation step
// ============================================================================

void
SimulationController::SimulationStep()
{
    if (!SimulationSync::HasPendingStep())
    {
        return;
    }

    /*
     * SimulationStep permanece responsável pela sincronização
     * SUMO -> ns-3 e pela exportação do estado.
     *
     * CamScheduler::Run() foi removido daqui porque agora possui
     * uma cadência temporal independente em SendCam().
     */
    SimulationStep::Execute();

    SimulationSync::ClearPendingStep();
}

// ============================================================================
// SUMO → ns-3 synchronization
// ============================================================================

void
SimulationController::HandleVehicleUpdateFromSumo(
    const VehicleState& parsedVs)
{
    SimulationSync::ProcessVehicle(parsedVs);
}

} // namespace ns3
