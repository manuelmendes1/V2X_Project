#include "../vanetza_integration/denm/trigger/denm_trigger.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace ns3;

int main()
{
    std::cout << "===== DENM TRIGGER TEST =====\n";

    uint32_t callbackCount = 0;
    DenmTriggerRequest received;

    DenmTrigger trigger(
        [&](const DenmTriggerRequest& request)
        {
            ++callbackCount;
            received = request;
        });

    DenmTriggerRequest valid;
    valid.source = DenmTriggerSource::TEST;
    valid.eventType = DenmTriggerEventType::HAZARD_DETECTED;
    valid.vehicleId = "veh0";
    valid.eventId = "event-A";
    valid.x = 10.0;
    valid.y = 20.0;
    valid.causeCode = 94;
    valid.relevanceDistanceMeters = 250.0;
    valid.validityDurationMs = 5000;
    valid.triggerTimestampUs = 123456;
    valid.reason = "deterministic_test";

    assert(trigger.Submit(valid));
    assert(callbackCount == 1);
    assert(received.source == valid.source);
    assert(received.eventType == valid.eventType);
    assert(received.vehicleId == valid.vehicleId);
    assert(received.eventId == valid.eventId);
    assert(received.x == valid.x);
    assert(received.y == valid.y);
    assert(received.causeCode == valid.causeCode);
    assert(received.relevanceDistanceMeters == valid.relevanceDistanceMeters);
    assert(received.validityDurationMs == valid.validityDurationMs);
    assert(received.triggerTimestampUs == valid.triggerTimestampUs);
    assert(received.reason == valid.reason);

    std::cout << "PASS valid_request\n";

    DenmTriggerRequest missingCause = valid;
    missingCause.causeCode = 0;

    assert(!trigger.Submit(missingCause));
    assert(callbackCount == 1);

    std::cout << "PASS missing_cause_rejected\n";

    DenmTriggerRequest missingVehicle = valid;
    missingVehicle.vehicleId.clear();

    assert(!trigger.Submit(missingVehicle));
    assert(callbackCount == 1);

    std::cout << "PASS missing_vehicle_id_rejected\n";

    DenmTriggerRequest missingEvent = valid;
    missingEvent.eventId.clear();

    assert(!trigger.Submit(missingEvent));
    assert(callbackCount == 1);

    std::cout << "PASS missing_event_id_rejected\n";


    DenmTriggerRequest missingTimestamp = valid;
    missingTimestamp.triggerTimestampUs = 0;

    assert(!trigger.Submit(missingTimestamp));
    assert(callbackCount == 1);

    std::cout << "PASS missing_timestamp_rejected\n";

    DenmTriggerRequest missingValidity = valid;
    missingValidity.validityDurationMs = 0;

    assert(!trigger.Submit(missingValidity));
    assert(callbackCount == 1);

    std::cout << "PASS missing_validity_rejected\n";

    DenmTriggerRequest invalidPosition = valid;
    invalidPosition.x = std::numeric_limits<double>::quiet_NaN();

    assert(!trigger.Submit(invalidPosition));
    assert(callbackCount == 1);

    std::cout << "PASS invalid_position_rejected\n";

    DenmTrigger noConsumer;

    assert(!noConsumer.Submit(valid));

    std::cout << "PASS missing_consumer_rejected\n";

    DenmTriggerRequest cleared = valid;
    cleared.eventType = DenmTriggerEventType::HAZARD_CLEARED;
    cleared.validityDurationMs = 0;

    assert(trigger.Submit(cleared));
    assert(callbackCount == 2);

    std::cout << "PASS hazard_cleared_without_validity\n";

    std::cout << "DENM_TRIGGER_TEST_EXIT=0\n";
    return 0;
}
