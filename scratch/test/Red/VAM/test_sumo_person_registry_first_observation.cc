#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"

#include <cstdint>
#include <iostream>
#include <string>

using namespace ns3;

namespace
{

bool Check(bool condition, const char* description)
{
    std::cout
        << (condition ? "[PASS] " : "[FAIL] ")
        << description
        << '\n';

    return condition;
}

SumoPersonDatagram Upsert(
    const std::string& personId,
    std::uint64_t timestampUs,
    double x)
{
    SumoPersonDatagram datagram;
    datagram.eventType = SumoPersonEventType::Upsert;
    datagram.personId = personId;
    datagram.sample = SumoPersonSample{x, 20.0, 1.2, 90.0};
    datagram.simulationTimeUs = timestampUs;
    datagram.sumoVClass = "pedestrian";
    return datagram;
}

SumoPersonDatagram Remove(
    const std::string& personId,
    std::uint64_t timestampUs)
{
    SumoPersonDatagram datagram;
    datagram.eventType = SumoPersonEventType::Remove;
    datagram.personId = personId;
    datagram.sample.reset();
    datagram.simulationTimeUs = timestampUs;
    return datagram;
}

} // namespace

int main()
{
    SumoPersonRegistry registry;
    bool ok = true;

    // First accepted UPSERT establishes the beginning of this active
    // observation period.
    ok &= Check(
        registry.Apply(Upsert("person-1", 100, 10.0)),
        "first UPSERT is accepted");

    auto state = registry.Find("person-1");
    ok &= Check(
        state.has_value() &&
            state->firstObservedAtUs == 100 &&
            state->simulationTimeUs == 100,
        "first observation timestamp is initialized");

    // Further samples update mobility and latest timestamp, but must not
    // reset the beginning of the active observation period.
    ok &= Check(
        registry.Apply(Upsert("person-1", 200, 12.0)),
        "subsequent UPSERT is accepted");

    state = registry.Find("person-1");
    ok &= Check(
        state.has_value() &&
            state->firstObservedAtUs == 100 &&
            state->simulationTimeUs == 200 &&
            state->sample.x == 12.0,
        "subsequent UPSERT preserves first observation");

    // Out-of-order samples must not modify either timestamp or mobility.
    ok &= Check(
        !registry.Apply(Upsert("person-1", 150, 99.0)),
        "out-of-order UPSERT is rejected");

    state = registry.Find("person-1");
    ok &= Check(
        state.has_value() &&
            state->firstObservedAtUs == 100 &&
            state->simulationTimeUs == 200 &&
            state->sample.x == 12.0,
        "rejected UPSERT leaves observation history unchanged");

    // An accepted REMOVE ends the active observation period.
    ok &= Check(
        registry.Apply(Remove("person-1", 300)),
        "REMOVE is accepted");

    ok &= Check(
        !registry.Find("person-1").has_value(),
        "removed person is no longer active");

    // The existing tombstone must reject a delayed UPSERT.
    ok &= Check(
        !registry.Apply(Upsert("person-1", 250, 50.0)),
        "delayed UPSERT after REMOVE is rejected");

    // A later UPSERT starts a new active observation period.
    ok &= Check(
        registry.Apply(Upsert("person-1", 400, 30.0)),
        "re-entry UPSERT is accepted");

    state = registry.Find("person-1");
    ok &= Check(
        state.has_value() &&
            state->firstObservedAtUs == 400 &&
            state->simulationTimeUs == 400,
        "re-entry initializes a new first observation timestamp");

    ok &= Check(
        registry.Apply(Upsert("person-1", 500, 31.0)),
        "post-re-entry UPSERT is accepted");

    state = registry.Find("person-1");
    ok &= Check(
        state.has_value() &&
            state->firstObservedAtUs == 400 &&
            state->simulationTimeUs == 500,
        "new observation period remains stable");

    registry.Clear();

    ok &= Check(
        registry.Empty() && registry.Size() == 0,
        "Clear removes active observation history");

    // Clear must also remove the tombstone. A timestamp older than the
    // previous REMOVE must be accepted in the new registry lifecycle.
    ok &= Check(
        registry.Apply(Upsert("person-1", 50, 60.0)),
        "UPSERT after Clear is accepted with reset tombstone");

    state = registry.Find("person-1");
    ok &= Check(
        state.has_value() &&
            state->firstObservedAtUs == 50 &&
            state->simulationTimeUs == 50,
        "UPSERT after Clear starts fresh observation history");

    if (!ok)
    {
        std::cerr
            << "SUMO_PERSON_REGISTRY_FIRST_OBSERVATION_HISTORY=FAIL\n";
        return 1;
    }

    std::cout
        << "SUMO_PERSON_REGISTRY_FIRST_OBSERVATION_HISTORY=PASS\n";
    return 0;
}
