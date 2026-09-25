#include "vanetza_integration/denm/scheduler/denm_scheduler_event_store.h"

#include <iostream>
#include <string>

namespace
{

int failures = 0;

void
Expect(
    bool condition,
    const std::string& description)
{
    if (condition)
    {
        std::cout << "PASS " << description << std::endl;
    }
    else
    {
        std::cout << "FAIL " << description << std::endl;
        ++failures;
    }
}

} // namespace

int main() {

    std::cout << "===== DENM SCHEDULER EVENT STORE TEST ====="
              << std::endl;

    ns3::DenmScheduledEventStore store;

    ns3::DenmScheduledEvent eventA;
    eventA.triggerRequest.vehicleId = "veh0";
    eventA.triggerRequest.eventId = "event-A";

    ns3::DenmScheduledEvent eventB;
    eventB.triggerRequest.vehicleId = "veh0";
    eventB.triggerRequest.eventId = "event-B";

    const ns3::DenmEventId idA{
        "veh0",
        "event-A"};

    const ns3::DenmEventId idB{
        "veh0",
        "event-B"};

    const bool insertedA =
        store.Insert(idA, eventA);

    Expect(
        insertedA,
        "first event is inserted");

    const bool insertedB =
        store.Insert(idB, eventB);

    Expect(
        insertedB,
        "second event from same vehicle is inserted independently");

    Expect(
        store.Size() == 2,
        "two events from the same vehicle coexist");

    const auto* foundA =
        store.Find(idA);

    const auto* foundB =
        store.Find(idB);

    Expect(
        foundA != nullptr,
        "event-A can be found independently");

    Expect(
        foundB != nullptr,
        "event-B can be found independently");

    Expect(
        foundA != nullptr &&
        foundA->triggerRequest.eventId == "event-A",
        "event-A retains its identity");

    Expect(
        foundB != nullptr &&
        foundB->triggerRequest.eventId == "event-B",
        "event-B retains its identity");

    Expect(
        store.IsInitialTransmissionPending(idA),
        "newly scheduled event-A is pending initial transmission");

    Expect(
        store.IsInitialTransmissionPending(idB),
        "newly scheduled event-B is pending initial transmission");

    Expect(
        store.MarkInitialTransmissionComplete(idA),
        "event-A initial transmission can be marked complete");

    Expect(
        !store.IsInitialTransmissionPending(idA),
        "event-A is no longer pending after initial transmission");

    Expect(
        store.IsInitialTransmissionPending(idB),
        "event-B remains pending independently");

    ns3::DenmScheduledEvent eventC;
    eventC.triggerRequest.vehicleId = "veh1";
    eventC.triggerRequest.eventId = "event-C";

    const ns3::DenmEventId idC{
        "veh1",
        "event-C"};

    Expect(
        store.Insert(idC, eventC),
        "event-C from another vehicle is inserted");

    const auto veh0Events =
        store.GetEventIdsForVehicle("veh0");

    const auto veh1Events =
        store.GetEventIdsForVehicle("veh1");

    Expect(
        veh0Events.size() == 2,
        "vehicle veh0 has exactly two scheduled events");

    Expect(
        veh1Events.size() == 1,
        "vehicle veh1 has exactly one scheduled event");

    Expect(
        (veh0Events[0] == idA ||
         veh0Events[0] == idB) &&
        (veh0Events[1] == idA ||
         veh0Events[1] == idB) &&
        veh0Events[0] != veh0Events[1],
        "veh0 event enumeration contains both independent event identities");

    Expect(
        veh1Events[0] == idC,
        "veh1 event enumeration contains event-C");

    Expect(
        store.Erase(idA),
        "event-A can be removed independently");

    Expect(
        store.Size() == 2,
        "removing event-A leaves event-B and event-C scheduled");

    Expect(
        store.Find(idA) == nullptr,
        "event-A is absent after erase");

    Expect(
        store.Find(idB) != nullptr,
        "event-B remains scheduled after event-A removal");

    Expect(
        store.Find(idC) != nullptr,
        "event-C remains scheduled after event-A removal");

    std::cout
        << "DENM_SCHEDULER_EVENT_STORE_EXIT="
        << failures
        << std::endl;

    return failures == 0 ? 0 : 1;
}
