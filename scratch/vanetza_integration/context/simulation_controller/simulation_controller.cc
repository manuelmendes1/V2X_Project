#include "simulation_controller.h"

#include "vanetza_integration/cam/scheduler/cam_scheduler.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/denm/scheduler/denm_scheduler.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/simulation_step/simulation_step.h"
#include "vanetza_integration/context/simulation_sync/simulation_sync.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"

#include "ns3/simulator.h"

#include <chrono>

#include <cstdint>
#include <iostream>

namespace ns3
{

Time SimulationController::m_stepLength = MilliSeconds(100);

bool SimulationController::m_denmEnabled = false;

double SimulationController::m_denmRadiusMeters = 1000.0;

DenmScenarioEventSource SimulationController::m_denmScenarioEvents{};


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

    // DENM é explicitamente opt-in e não altera o comportamento CAM.
    if (m_denmEnabled)
    {
        DenmScheduler::Reset();
        DenmScheduler::SetCompatibilityFallbackEnabled(false);
        m_denmScenarioEvents.Clear();

        m_denmScenarioEvents.SetSubmissionTimingCallback(
            [](const DenmScenarioEvent& event,
               uint64_t triggerTimestampUs,
               uint64_t submissionTimestampUs,
               uint64_t)
            {
                std::cout
                    << "[DENM_TRIGGER_TIMING]"
                    << " vehicle=" << event.vehicleId
                    << " event=" << event.eventId
                    << " trigger_us=" << triggerTimestampUs
                    << " submit_sim_us=" << submissionTimestampUs
                    << '\n';
            });

        Simulator::ScheduleNow(
            &SimulationController::SendDenm);
    }
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

    Simulator::Schedule(
        CamScheduler::RunInterval(),
        &SimulationController::SendCam);
}

void
SimulationController::SetDenmEnabled(bool enabled)
{
    m_denmEnabled = enabled;
}

void
SimulationController::SetDenmRadiusMeters(double radiusMeters)
{
    m_denmRadiusMeters = radiusMeters;
}

void
SimulationController::SetDenmPkiDirectory(const std::string& directory)
{
    VehicleSecurityInitializer::SetDenmPkiDirectory(directory);
}


void
SimulationController::SendDenm()
{
    if (!m_denmEnabled)
    {
        return;
    }

    m_denmScenarioEvents.Evaluate(
        static_cast<uint64_t>(
            Simulator::Now().GetMicroSeconds()));

    DenmScheduler::Run(
        m_denmRadiusMeters);

    Simulator::Schedule(
        DenmScheduler::RunInterval(),
        &SimulationController::SendDenm);
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
SimulationController::ConfigureSumoAdapterCallbacks()
{
    SumoAdapter::SetVehicleUpdateCallback(
        &SimulationController::HandleVehicleUpdateFromSumo);

    SumoAdapter::SetDenmEventCallback(
        &SimulationController::HandleDenmEventFromSumo);
}

void
SimulationController::HandleVehicleUpdateFromSumo(
    const VehicleState& parsedVs)
{
    SimulationSync::ProcessVehicle(parsedVs);
}


void
SimulationController::HandleDenmEventFromSumo(
    const DenmScenarioEvent& event)
{
    const uint64_t ingestSimUs =
        static_cast<uint64_t>(
            Simulator::Now().GetMicroSeconds());

    const uint64_t ingestMonotonicUs =
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

    std::cout
        << "[DENM_EVENT_INGEST]"
        << " vehicle=" << event.vehicleId
        << " event=" << event.eventId
        << " trigger_us=" << event.triggerTimeUs
        << " ingest_sim_us=" << ingestSimUs
        << " ingest_monotonic_us=" << ingestMonotonicUs
        << '\n';

    m_denmScenarioEvents.AddEvent(event);
}

std::size_t
SimulationController::DenmScenarioEventCount()
{
    return m_denmScenarioEvents.EventCount();
}

std::size_t
SimulationController::PendingDenmScenarioEventCount()
{
    return m_denmScenarioEvents.PendingEventCount();
}

void
SimulationController::ResetDenmScenarioEvents()
{
    m_denmScenarioEvents.Clear();
}

} // namespace ns3
