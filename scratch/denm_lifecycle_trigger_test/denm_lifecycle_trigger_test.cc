#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/action/denm_action_id_manager.h"
#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"

#include <iostream>

using namespace ns3;

namespace
{

DenmTriggerRequest
MakeRequest(
    const char* vehicleId,
    const char* eventId,
    DenmTriggerEventType eventType)
{
    DenmTriggerRequest request;

    request.source =
        DenmTriggerSource::TEST;

    request.eventType =
        eventType;

    request.vehicleId =
        vehicleId;

    request.eventId =
        eventId;

    request.x =
        10.0;

    request.y =
        20.0;

    request.causeCode =
        94;

    request.relevanceDistanceMeters =
        250.0;

    request.validityDurationMs =
        5000;

    request.triggerTimestampUs =
        1000000;

    request.reason =
        "lifecycle_test";

    return request;
}

bool
Expect(
    bool condition,
    const char* description)
{
    if (condition)
    {
        std::cout
            << "PASS "
            << description
            << "\n";

        return true;
    }

    std::cout
        << "FAIL "
        << description
        << "\n";

    return false;
}

} // namespace

int main()
{
    bool ok = true;

    DenmActionIdManager::Instance().Reset();

    DenmLifecycleManager lifecycle;

    VehicleState veh0;
    veh0.identity.vehId =
        "veh0";
    veh0.identity.stationId =
        100;

    VehicleState veh1;
    veh1.identity.vehId =
        "veh1";
    veh1.identity.stationId =
        200;

    DenmLifecycleResult result;

    std::cout
        << "===== DENM LIFECYCLE TRIGGER TEST =====\n";

    // ------------------------------------------------------------
    // NEW
    // ------------------------------------------------------------

    const auto newRequest =
        MakeRequest(
            "veh0",
            "event-A",
            DenmTriggerEventType::HAZARD_DETECTED);

    ok &= Expect(
        lifecycle.Process(
            veh0,
            newRequest,
            result),
        "NEW accepted");

    ok &= Expect(
        result.action ==
            DenmManagementAction::NEW,
        "NEW action selected");

    ok &= Expect(
        result.actionId.originatingStationId == 100 &&
        result.actionId.sequenceNumber != 0,
        "NEW ActionID valid");

    const DenmActionId actionA =
        result.actionId;

    // ------------------------------------------------------------
    // UPDATE
    // ------------------------------------------------------------

    const auto updateRequest =
        MakeRequest(
            "veh0",
            "event-A",
            DenmTriggerEventType::HAZARD_UPDATED);

    ok &= Expect(
        lifecycle.Process(
            veh0,
            updateRequest,
            result),
        "UPDATE accepted");

    ok &= Expect(
        result.action ==
            DenmManagementAction::UPDATE,
        "UPDATE action selected");

    ok &= Expect(
        result.actionId.originatingStationId ==
            actionA.originatingStationId &&
        result.actionId.sequenceNumber ==
            actionA.sequenceNumber,
        "UPDATE reuses same ActionID");

    // ------------------------------------------------------------
    // CANCELLATION
    // ------------------------------------------------------------

    const auto clearRequest =
        MakeRequest(
            "veh0",
            "event-A",
            DenmTriggerEventType::HAZARD_CLEARED);

    ok &= Expect(
        lifecycle.Process(
            veh0,
            clearRequest,
            result),
        "CANCELLATION accepted");

    ok &= Expect(
        result.action ==
            DenmManagementAction::CANCELLATION,
        "CANCELLATION action selected");

    ok &= Expect(
        result.actionId.originatingStationId ==
            actionA.originatingStationId &&
        result.actionId.sequenceNumber ==
            actionA.sequenceNumber,
        "CANCELLATION reuses same ActionID");

    ok &= Expect(
        !DenmActionIdManager::Instance().IsActive(
            actionA),
        "CANCELLATION closes ActionID");

    // ------------------------------------------------------------
    // UPDATE after termination
    // ------------------------------------------------------------

    ok &= Expect(
        !lifecycle.Process(
            veh0,
            updateRequest,
            result),
        "UPDATE after termination rejected");

    // ------------------------------------------------------------
    // Independent event on same vehicle
    // ------------------------------------------------------------

    const auto eventBRequest =
        MakeRequest(
            "veh0",
            "event-B",
            DenmTriggerEventType::HAZARD_DETECTED);

    ok &= Expect(
        lifecycle.Process(
            veh0,
            eventBRequest,
            result),
        "independent event accepted");

    ok &= Expect(
        result.action ==
            DenmManagementAction::NEW,
        "independent event creates NEW");

    const DenmActionId actionB =
        result.actionId;

    ok &= Expect(
        actionB.originatingStationId == 100 &&
        actionB.sequenceNumber != 0 &&
        actionB.sequenceNumber != actionA.sequenceNumber,
        "independent event gets different ActionID");

    // ------------------------------------------------------------
    // Different vehicle
    // ------------------------------------------------------------

    const auto otherVehicleRequest =
        MakeRequest(
            "veh1",
            "event-A",
            DenmTriggerEventType::HAZARD_DETECTED);

    ok &= Expect(
        lifecycle.Process(
            veh1,
            otherVehicleRequest,
            result),
        "different vehicle event accepted");

    ok &= Expect(
        result.action ==
            DenmManagementAction::NEW,
        "different vehicle creates NEW");

    const DenmActionId actionC =
        result.actionId;

    ok &= Expect(
        actionC.originatingStationId == 200 &&
        actionC.sequenceNumber != 0,
        "different vehicle has independent ActionID");

    // ------------------------------------------------------------
    // Duplicate NEW
    // ------------------------------------------------------------

    const auto duplicateNew =
        MakeRequest(
            "veh0",
            "event-B",
            DenmTriggerEventType::HAZARD_DETECTED);

    ok &= Expect(
        !lifecycle.Process(
            veh0,
            duplicateNew,
            result),
        "duplicate NEW for active event rejected");

    ok &= Expect(
        DenmActionIdManager::Instance().IsActive(
            actionB),
        "active event-B ActionID remains active");

    // ------------------------------------------------------------
    // Abort / local cleanup
    // ------------------------------------------------------------

    DenmActionId abortActionId;
    ok &= Expect(
        lifecycle.GetActionId(
            "veh0",
            "event-B",
            abortActionId),
        "active event-B has lifecycle ActionID before abort");

    ok &= Expect(
        lifecycle.Abort(
            "veh0",
            "event-B"),
        "Abort closes active event");

    ok &= Expect(
        !DenmActionIdManager::Instance().IsActive(
            abortActionId),
        "Abort closes ActionID");

    DenmActionId afterAbortActionId;
    ok &= Expect(
        !lifecycle.GetActionId(
            "veh0",
            "event-B",
            afterAbortActionId),
        "Abort removes event mapping");

    ok &= Expect(
        !lifecycle.Abort(
            "veh0",
            "event-B"),
        "Abort of unknown event is rejected");

    // ------------------------------------------------------------
    // Identity mismatch
    // ------------------------------------------------------------

    const auto mismatchRequest =
        MakeRequest(
            "veh1",
            "event-X",
            DenmTriggerEventType::HAZARD_DETECTED);

    ok &= Expect(
        !lifecycle.Process(
            veh0,
            mismatchRequest,
            result),
        "vehicle identity mismatch rejected");

    // ------------------------------------------------------------
    // Lifecycle reset owns ActionID cleanup
    // ------------------------------------------------------------

    ok &= Expect(
        DenmActionIdManager::Instance().IsActive(
            actionC),
        "active event from veh1 exists before lifecycle reset");

    lifecycle.Reset();

    ok &= Expect(
        !DenmActionIdManager::Instance().IsActive(
            actionC),
        "LifecycleManager reset closes ActionID state");

    if (!ok)
    {
        std::cout
            << "DENM_LIFECYCLE_TRIGGER_TEST_EXIT=1\n";

        return 1;
    }

    std::cout
        << "DENM_LIFECYCLE_TRIGGER_TEST_EXIT=0\n";

    return 0;
}
