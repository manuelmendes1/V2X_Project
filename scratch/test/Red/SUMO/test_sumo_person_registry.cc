#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace
{

bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    return condition;
}

ns3::SumoPersonDatagram Upsert(
    const std::string& personId,
    double x,
    double y,
    double speed,
    double angleDeg,
    std::uint64_t timestampUs)
{
    ns3::SumoPersonDatagram datagram;
    datagram.eventType = ns3::SumoPersonEventType::Upsert;
    datagram.personId = personId;
    datagram.sample = ns3::SumoPersonSample{x, y, speed, angleDeg};
    datagram.simulationTimeUs = timestampUs;
    return datagram;
}

ns3::SumoPersonDatagram Remove(
    const std::string& personId,
    std::uint64_t timestampUs)
{
    ns3::SumoPersonDatagram datagram;
    datagram.eventType = ns3::SumoPersonEventType::Remove;
    datagram.personId = personId;
    datagram.sample.reset();
    datagram.simulationTimeUs = timestampUs;
    return datagram;
}

} // namespace

int main()
{
    bool passed = true;
    ns3::SumoPersonRegistry registry;

    // Each equal-timestamp case uses an isolated registry so a RED failure
    // cannot alter the state used by the existing contract checks below.
    {
        ns3::SumoPersonRegistry duplicateUpsertRegistry;
        const bool initialAccepted = duplicateUpsertRegistry.Apply(
            Upsert("ped-duplicate-upsert", 1.0, 2.0, 1.0, 45.0, 900));
        const bool duplicateAccepted = duplicateUpsertRegistry.Apply(
            Upsert("ped-duplicate-upsert", 9.0, 8.0, 1.0, 90.0, 900));
        const auto retained = duplicateUpsertRegistry.Find("ped-duplicate-upsert");

        passed = Check(
            initialAccepted && !duplicateAccepted && retained.has_value() &&
                std::abs(retained->sample.x - 1.0) < 1e-9 &&
                retained->simulationTimeUs == 900,
            "DUPLICATE_TIMESTAMP_UPSERT_REJECTED_WITHOUT_MUTATION") && passed;
    }

    {
        ns3::SumoPersonRegistry equalTimestampRemoveRegistry;
        const bool initialAccepted = equalTimestampRemoveRegistry.Apply(
            Upsert("ped-equal-remove", 3.0, 4.0, 1.0, 45.0, 1000));
        const bool removeAccepted = equalTimestampRemoveRegistry.Apply(
            Remove("ped-equal-remove", 1000));
        const auto retained = equalTimestampRemoveRegistry.Find("ped-equal-remove");

        passed = Check(
            initialAccepted && !removeAccepted && retained.has_value() &&
                std::abs(retained->sample.x - 3.0) < 1e-9 &&
                retained->simulationTimeUs == 1000,
            "EQUAL_TIMESTAMP_REMOVE_CANNOT_DELETE_CURRENT_SAMPLE") && passed;
    }

    {
        ns3::SumoPersonRegistry equalTimestampResurrectionRegistry;
        const bool initialAccepted = equalTimestampResurrectionRegistry.Apply(
            Upsert("ped-equal-resurrection", 5.0, 6.0, 1.0, 45.0, 1100));
        const bool removeAccepted = equalTimestampResurrectionRegistry.Apply(
            Remove("ped-equal-resurrection", 1200));
        const bool resurrectionAccepted = equalTimestampResurrectionRegistry.Apply(
            Upsert("ped-equal-resurrection", 7.0, 8.0, 1.0, 90.0, 1200));

        passed = Check(
            initialAccepted && removeAccepted && !resurrectionAccepted &&
                !equalTimestampResurrectionRegistry.Find("ped-equal-resurrection"),
            "EQUAL_TIMESTAMP_UPSERT_CANNOT_RESURRECT_REMOVED_PERSON") && passed;
    }

    passed = Check(
        registry.Empty() && registry.Size() == 0,
        "REGISTRY_STARTS_EMPTY") && passed;

    passed = Check(
        registry.Apply(Upsert("ped0", 10.0, 20.0, 1.4, 90.0, 100)),
        "UPSERT_INSERTS_PERSON") && passed;

    auto ped0 = registry.Find("ped0");
    passed = Check(
        ped0.has_value() &&
            std::abs(ped0->sample.x - 10.0) < 1e-9 &&
            std::abs(ped0->sample.y - 20.0) < 1e-9 &&
            std::abs(ped0->sample.speed - 1.4) < 1e-9 &&
            std::abs(ped0->sample.angleDeg - 90.0) < 1e-9 &&
            ped0->simulationTimeUs == 100,
        "UPSERT_STORES_SAMPLE_AND_LOGICAL_TIME") && passed;

    passed = Check(
        registry.Apply(Upsert("ped0", 11.0, 22.0, 1.8, 95.0, 200)) &&
            registry.Size() == 1,
        "UPSERT_UPDATES_EXISTING_PERSON_WITHOUT_DUPLICATING") && passed;

    ped0 = registry.Find("ped0");
    passed = Check(
        ped0.has_value() &&
            std::abs(ped0->sample.x - 11.0) < 1e-9 &&
            ped0->simulationTimeUs == 200,
        "LATEST_UPSERT_REPLACES_PREVIOUS_SAMPLE") && passed;

    passed = Check(
        registry.Apply(Upsert("ped1", 30.0, 40.0, 0.8, 180.0, 200)) &&
            registry.Size() == 2,
        "DISTINCT_PERSON_IDS_HAVE_INDEPENDENT_ENTRIES") && passed;

    passed = Check(
        !registry.Apply(Remove("ped0", 150)) &&
            registry.Find("ped0").has_value(),
        "STALE_REMOVE_CANNOT_DELETE_NEWER_SAMPLE") && passed;

    passed = Check(
        registry.Apply(Remove("ped0", 300)) &&
            !registry.Find("ped0").has_value() &&
            registry.Find("ped1").has_value() &&
            registry.Size() == 1,
        "REMOVE_DELETES_ONLY_MATCHING_PERSON") && passed;

    passed = Check(
        !registry.Apply(Upsert("ped0", 99.0, 99.0, 1.0, 0.0, 250)) &&
            !registry.Find("ped0").has_value(),
        "STALE_UPSERT_CANNOT_RESURRECT_REMOVED_PERSON") && passed;

    passed = Check(
        registry.Apply(Remove("unknown", 400)) && registry.Size() == 1,
        "REMOVE_OF_UNKNOWN_PERSON_IS_IDEMPOTENT") && passed;

    passed = Check(
        registry.Apply(Upsert("ped0", 1.0, 2.0, 1.0, 45.0, 401)) &&
            registry.Find("ped0").has_value(),
        "NEWER_UPSERT_CAN_REENTER_AFTER_REMOVE") && passed;

    ns3::SumoPersonDatagram missingSample;
    missingSample.eventType = ns3::SumoPersonEventType::Upsert;
    missingSample.personId = "ped-missing";
    missingSample.simulationTimeUs = 500;
    passed = Check(
        !registry.Apply(missingSample) && !registry.Find("ped-missing"),
        "UPSERT_WITHOUT_SAMPLE_REJECTED_AT_REGISTRY") && passed;

    passed = Check(
        !registry.Apply(Upsert(
            "ped-nan", std::numeric_limits<double>::quiet_NaN(),
            0.0, 1.0, 0.0, 500)) &&
            !registry.Find("ped-nan"),
        "NON_FINITE_SAMPLE_REJECTED_AT_REGISTRY") && passed;

    passed = Check(
        !registry.Apply(Upsert("ped-negative", 0.0, 0.0, -1.0, 0.0, 500)) &&
            !registry.Find("ped-negative"),
        "NEGATIVE_SPEED_REJECTED_AT_REGISTRY") && passed;

    auto invalidRemove = Remove("ped0", 600);
    invalidRemove.sample = ns3::SumoPersonSample{};
    passed = Check(
        !registry.Apply(invalidRemove),
        "REMOVE_WITH_MOBILITY_SAMPLE_REJECTED") && passed;

    passed = Check(
        !registry.Apply(Upsert(
            "ped-sentinel", -1073741824.0, 1.0, 1.0, 0.0, 600)) &&
            !registry.Find("ped-sentinel"),
        "TRACI_UNAVAILABLE_SENTINEL_REJECTED_AT_REGISTRY") && passed;

    passed = Check(
        !registry.Apply(Upsert("", 0.0, 0.0, 1.0, 0.0, 700)),
        "EMPTY_PERSON_ID_REJECTED_AT_REGISTRY") && passed;

    auto unknownEvent = Remove("ped0", 800);
    unknownEvent.eventType = static_cast<ns3::SumoPersonEventType>(255);
    passed = Check(
        !registry.Apply(unknownEvent),
        "UNKNOWN_EVENT_TYPE_REJECTED_AT_REGISTRY") && passed;

    registry.Clear();
    passed = Check(
        registry.Empty() && registry.Size() == 0 && !registry.Find("ped1"),
        "CLEAR_REMOVES_ACTIVE_AND_TOMBSTONE_STATE") && passed;

    std::cout << "SUMO_PERSON_REGISTRY=" << (passed ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}
