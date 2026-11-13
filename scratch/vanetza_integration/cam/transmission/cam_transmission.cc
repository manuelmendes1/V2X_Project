#include "cam_transmission.h"
#include "cam_transmission_request.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/core/v2x_log.h"

#include <cmath>
#include <cstdlib>
#include <string>

namespace {

    constexpr unsigned CAM_TEST_MAX_PACKETS_PER_VEHICLE = 40;

    /*
     * Requests CAM every 1 ms in experimental mode.
     */
    constexpr double CAM_TEST_INTERVAL = 0.001;

    /*
     * DCC Relaxed transmission period currently used by
     * Vanetza::dcc::StateMachineBudget.
     */
    constexpr double CAM_TEST_PHASE_PERIOD = 0.100;

    /*
     * One deterministic phase slot per millisecond.
     */
    constexpr double CAM_TEST_PHASE_STEP = 0.001;

    constexpr double CAM_EPSILON = 1e-9;

} // namespace

namespace ns3 {

    bool CamTransmission::HighRateTestEnabled() {
        const char* enabled = std::getenv("V2X_CAM_HIGH_RATE_TEST");
        return enabled != nullptr && std::string(enabled) == "1";
    }

    double CamTransmission::TestSchedulerInterval() {
        return CAM_TEST_INTERVAL;
    }

    double
    CamTransmission::TestTransmissionPhase(
        const VehicleState& vehicle)
    {
        /*
         * The nodeId is the authoritative unique ns-3 identifier.
         *
         * Map node 0..99 to phases 0..99 ms inside the
         * 100 ms DCC period. Higher node IDs reuse the phase.
         */
        const uint32_t phaseIndex =
            vehicle.identity.nodeId % 100;

        return phaseIndex *
               CAM_TEST_PHASE_STEP;
    }

    CamTransmissionRequest
    CamTransmission::PrepareTransmission(
        VehicleState& vehicle,
        double now)
    {
        CamTransmissionRequest req;

        req.vehicle = &vehicle;

        /*
         * --------------------------------------------------------------
         * Experimental high-rate CAM mode
         * --------------------------------------------------------------
         *
         * The scheduler is called every 1 ms in this mode.
         *
         * The first CAM of each vehicle is deliberately aligned to a
         * deterministic phase inside the 100 ms DCC period:
         *
         *   node 0  -> 0 ms
         *   node 1  -> 1 ms
         *   ...
         *   node 99 -> 99 ms
         *
         * Once the first DCC transmission occurs, Vanetza FlowControl /
         * StateMachineBudget naturally maintains the corresponding
         * approximately 100 ms transmission period.
         */
        if (HighRateTestEnabled())
        {
            // Bound the probe independently for every vehicle.
            if (vehicle.camSequenceCounter >=
                CAM_TEST_MAX_PACKETS_PER_VEHICLE)
            {
                return req;
            }
            /*
             * First CAM:
             *
             * Do not send immediately when the vehicle becomes ready.
             * Wait until its absolute 100 ms phase is reached.
             */
            if (vehicle.lastCamTime < 0.0)
            {
                const double phase =
                    TestTransmissionPhase(vehicle);

                const double periodIndex =
                    std::floor(
                        now / CAM_TEST_PHASE_PERIOD);

                const double periodStart =
                    periodIndex *
                    CAM_TEST_PHASE_PERIOD;

                double targetTime =
                    periodStart + phase;

                /*
                 * If this vehicle became ready after its phase in the
                 * current 100 ms period, wait for the next occurrence.
                 */
                if (targetTime + CAM_EPSILON < now)
                {
                    targetTime +=
                        CAM_TEST_PHASE_PERIOD;
                }

                if (now + CAM_EPSILON < targetTime)
                {
                    return req;
                }

                req.firstCam = true;
                req.reason =
                    "FIRST_CAM_HIGH_RATE_TEST";
            }
            else
            {
                const double elapsed =
                    now - vehicle.lastCamTime;

                if (elapsed < CAM_TEST_INTERVAL)
                {
                    return req;
                }

                req.firstCam = false;
                req.reason =
                    "HIGH_RATE_TEST";
            }

            ++vehicle.camSequenceCounter;

            vehicle.currentTxSeqId =
                vehicle.camSequenceCounter;

            req.shouldSend = true;

            return req;
        }

        /*
         * --------------------------------------------------------------
         * Original CAM transmission policy
         * --------------------------------------------------------------
         */

        const double elapsed =
            (vehicle.lastCamTime < 0.0)
                ? 999.0
                : (now - vehicle.lastCamTime);

        if (elapsed < 0.099)
        {
            return req;
        }

        req.firstCam =
            (vehicle.lastCamTime < 0.0);

        req.reason =
            "FIRST_CAM";

        if (!req.firstCam)
        {
            if (!ShouldTransmit(
                    vehicle,
                    vehicle.lastCamSnapshot,
                    now,
                    req.reason))
            {
                return req;
            }
        }

        ++vehicle.camSequenceCounter;

        vehicle.currentTxSeqId =
            vehicle.camSequenceCounter;

        req.shouldSend = true;

        return req;
    }

    bool
    CamTransmission::ShouldTransmit(
        const VehicleState& current,
        const CamTriggerSnapshot& previous,
        double now,
        std::string& reason)
    {
        const double dx =
            current.mobility.position.x -
            previous.position.x;

        const double dy =
            current.mobility.position.y -
            previous.position.y;

        const double dist =
            std::sqrt(
                dx * dx +
                dy * dy);

        if (dist > 4.0)
        {
            reason = "DISTANCE";
            return true;
        }

        const double dSpeed =
            std::abs(
                current.mobility.speed -
                previous.speed);

        if (dSpeed > 0.5)
        {
            reason = "SPEED";
            return true;
        }

        const double dHeading =
            std::abs(
                current.mobility.heading -
                previous.heading);

        if (dHeading > 4.0)
        {
            reason = "HEADING";
            return true;
        }

        if (previous.lastCamTime < 0.0 ||
            (now - previous.lastCamTime) >= 1.0)
        {
            reason = "TIME";
            return true;
        }

        reason.clear();

        return false;
    }

    void
    CamTransmission::CommitTransmission(
        VehicleState& vehicle,
        double now)
    {
        vehicle.lastCamTime = now;
        vehicle.lastCamGenTime = now;

        vehicle.lastCamSnapshot.position =
            vehicle.mobility.position;

        vehicle.lastCamSnapshot.speed =
            vehicle.mobility.speed;

        vehicle.lastCamSnapshot.heading =
            vehicle.mobility.heading;

        vehicle.lastCamSnapshot.lastCamTime =
            now;
    }

} // namespace ns3
