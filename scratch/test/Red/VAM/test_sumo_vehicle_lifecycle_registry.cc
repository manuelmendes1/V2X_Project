#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"

#include <cstdint>
#include <iostream>
#include <optional>

using namespace ns3;

namespace
{
bool Check(bool condition, const char* description)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << description << '\n';
    return condition;
}
}

int main()
{
    SumoVehicleLifecycleRegistry registry;
    bool ok = true;

    ok &= Check(
        registry.IsDispatchEligible("manual-its"),
        "untracked ITS vehicle remains dispatch eligible");

    ok &= Check(
        registry.ApplyUpsert("veh-1", UINT64_C(100)),
        "first timestamped UPSERT is accepted");

    auto state = registry.Find("veh-1");
    ok &= Check(
        state.has_value() && state->active &&
            state->firstObservedAtUs == UINT64_C(100) &&
            state->lastEventTimeUs == UINT64_C(100),
        "first observation and latest event timestamps are initialized");

    ok &= Check(
        registry.IsDispatchEligible("veh-1"),
        "active SUMO vehicle remains dispatch eligible");

    ok &= Check(
        registry.ApplyUpsert("veh-1", UINT64_C(200)),
        "subsequent UPSERT is accepted");

    state = registry.Find("veh-1");
    ok &= Check(
        state.has_value() && state->active &&
            state->firstObservedAtUs == UINT64_C(100) &&
            state->lastEventTimeUs == UINT64_C(200),
        "subsequent UPSERT preserves first observation");

    ok &= Check(
        !registry.ApplyUpsert("veh-1", UINT64_C(200)),
        "duplicate timestamp UPSERT is rejected");

    ok &= Check(
        !registry.ApplyUpsert("veh-1", UINT64_C(150)),
        "out-of-order UPSERT is rejected");

    state = registry.Find("veh-1");
    ok &= Check(
        state.has_value() && state->active &&
            state->firstObservedAtUs == UINT64_C(100) &&
            state->lastEventTimeUs == UINT64_C(200),
        "rejected UPSERT leaves lifecycle history unchanged");

    ok &= Check(
        registry.ApplyRemove("veh-1", UINT64_C(300)),
        "REMOVE is accepted");

    state = registry.Find("veh-1");
    ok &= Check(
        state.has_value() && !state->active &&
            state->lastEventTimeUs == UINT64_C(300),
        "REMOVE marks the SUMO lifecycle inactive");

    ok &= Check(
        !registry.IsDispatchEligible("veh-1"),
        "removed SUMO vehicle is excluded from dispatch");

    ok &= Check(
        !registry.ApplyUpsert("veh-1", UINT64_C(250)),
        "delayed UPSERT after REMOVE is rejected");

    ok &= Check(
        registry.ApplyUpsert("veh-1", UINT64_C(400)),
        "later re-entry UPSERT is accepted");

    state = registry.Find("veh-1");
    ok &= Check(
        state.has_value() && state->active &&
            state->firstObservedAtUs == UINT64_C(400) &&
            state->lastEventTimeUs == UINT64_C(400),
        "re-entry starts a new first-observation period");

    // Legacy six-field datagrams have no logical timestamp.
    ok &= Check(
        registry.ApplyUpsert("veh-legacy", std::nullopt),
        "legacy timestamp-less UPSERT is accepted");

    state = registry.Find("veh-legacy");
    ok &= Check(
        state.has_value() && state->active &&
            !state->firstObservedAtUs.has_value() &&
            !state->lastEventTimeUs.has_value(),
        "legacy first observation remains explicitly untimed");

    ok &= Check(
        registry.ApplyUpsert("veh-legacy", UINT64_C(50)),
        "timestamped update after legacy UPSERT is accepted");

    state = registry.Find("veh-legacy");
    ok &= Check(
        state.has_value() && state->active &&
            !state->firstObservedAtUs.has_value() &&
            state->lastEventTimeUs == UINT64_C(50),
        "later timestamp does not invent the unknown first observation");

    ok &= Check(
        registry.ApplyUpsert("veh-legacy", std::nullopt),
        "timestamp-less update of active legacy vehicle is accepted");

    state = registry.Find("veh-legacy");
    ok &= Check(
        state.has_value() && state->active &&
            !state->firstObservedAtUs.has_value() &&
            state->lastEventTimeUs == UINT64_C(50),
        "timestamp-less update preserves known temporal metadata");

    ok &= Check(
        registry.ApplyRemove("veh-legacy", UINT64_C(60)),
        "legacy vehicle can be removed with explicit timestamp");

    ok &= Check(
        !registry.ApplyUpsert("veh-legacy", std::nullopt),
        "timestamp-less UPSERT cannot resurrect a removed vehicle");

    // A REMOVE may arrive before the first UPSERT over UDP.
    ok &= Check(
        registry.ApplyRemove("veh-unknown", UINT64_C(700)),
        "REMOVE for an unknown vehicle creates a tombstone");

    ok &= Check(
        !registry.IsDispatchEligible("veh-unknown"),
        "unknown removed vehicle is not dispatch eligible");

    ok &= Check(
        !registry.ApplyUpsert("veh-unknown", UINT64_C(650)),
        "delayed first UPSERT is rejected by unknown-vehicle tombstone");

    ok &= Check(
        registry.ApplyUpsert("veh-unknown", UINT64_C(800)),
        "newer UPSERT clears unknown-vehicle tombstone");

    state = registry.Find("veh-unknown");
    ok &= Check(
        state.has_value() && state->active &&
            state->firstObservedAtUs == UINT64_C(800),
        "unknown-vehicle re-entry initializes first observation");

    registry.Clear();

    ok &= Check(
        !registry.Find("veh-unknown").has_value() &&
            registry.IsDispatchEligible("veh-unknown"),
        "Clear removes lifecycle state and tombstones");

    ok &= Check(
        registry.ApplyUpsert("veh-unknown", UINT64_C(1)),
        "UPSERT after Clear starts a fresh lifecycle");

    state = registry.Find("veh-unknown");
    ok &= Check(
        state.has_value() &&
            state->firstObservedAtUs == UINT64_C(1),
        "UPSERT after Clear initializes a fresh first observation");

    if (!ok)
    {
        std::cerr << "SUMO_VEHICLE_LIFECYCLE_REGISTRY=FAIL\n";
        return 1;
    }

    std::cout << "SUMO_VEHICLE_LIFECYCLE_REGISTRY=PASS\n";
    return 0;
}
