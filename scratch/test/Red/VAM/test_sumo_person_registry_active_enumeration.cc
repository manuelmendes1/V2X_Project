#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <utility>

using namespace ns3;

using ExpectedActivePersons =
    std::map<std::string, SumoPersonState>;

using ActualActivePersons = decltype(
    std::declval<const SumoPersonRegistry&>().GetActivePersons());

static_assert(
    std::is_same_v<ActualActivePersons, const ExpectedActivePersons&>,
    "GetActivePersons must return a const reference to active persons");

static_assert(
    noexcept(std::declval<const SumoPersonRegistry&>().GetActivePersons()),
    "GetActivePersons must be noexcept");

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

    const auto& active = registry.GetActivePersons();

    ok &= Check(
        active.empty(),
        "empty registry exposes an empty active-person collection");

    ok &= Check(
        registry.Apply(Upsert("person-a", 100, 10.0)),
        "first person UPSERT is accepted");

    ok &= Check(
        registry.Apply(Upsert("person-b", 100, 20.0)),
        "second person UPSERT is accepted");

    ok &= Check(
        active.size() == 2 &&
            active.count("person-a") == 1 &&
            active.count("person-b") == 1,
        "enumeration exposes both active persons");

    ok &= Check(
        registry.Apply(Remove("person-a", 200)),
        "person REMOVE is accepted");

    ok &= Check(
        active.size() == 1 &&
            active.count("person-a") == 0 &&
            active.count("person-b") == 1,
        "enumeration excludes removed persons");

    ok &= Check(
        !registry.Apply(Upsert("person-a", 150, 99.0)),
        "stale UPSERT after REMOVE is rejected");

    ok &= Check(
        active.size() == 1 &&
            active.count("person-a") == 0,
        "rejected stale event does not change enumeration");

    ok &= Check(
        registry.Apply(Upsert("person-a", 300, 30.0)),
        "valid re-entry UPSERT is accepted");

    const auto reentered = active.find("person-a");

    ok &= Check(
        active.size() == 2 &&
            reentered != active.end() &&
            reentered->second.firstObservedAtUs == 300,
        "re-entry appears with a new observation period");

    registry.Clear();

    ok &= Check(
        active.empty() &&
            registry.Empty() &&
            registry.Size() == 0,
        "Clear empties the exposed active-person collection");

    if (!ok)
    {
        std::cerr
            << "SUMO_PERSON_REGISTRY_ACTIVE_ENUMERATION=FAIL\n";
        return 1;
    }

    std::cout
        << "SUMO_PERSON_REGISTRY_ACTIVE_ENUMERATION=PASS\n";
    return 0;
}
