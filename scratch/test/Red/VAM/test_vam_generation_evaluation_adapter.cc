#include "vanetza_integration/vam/perception/vam_generation_evaluation_adapter.h"

#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"
#include "vanetza_integration/context/sumo_vehicle_lifecycle_registry/sumo_vehicle_lifecycle_registry.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

using namespace ns3;
using namespace vanetza_integration::vam;
using namespace vanetza_integration::vru::role;

namespace
{

bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << label << '\n';
    return condition;
}

SumoPersonDatagram PersonUpsert(
    const std::string& personId,
    std::uint64_t timestampUs,
    double x,
    double y,
    double headingDeg,
    std::optional<std::string> vClass = std::nullopt)
{
    SumoPersonDatagram datagram{};
    datagram.eventType = SumoPersonEventType::Upsert;
    datagram.personId = personId;
    datagram.sample = SumoPersonSample{
        x, y, 1.0, headingDeg};
    datagram.simulationTimeUs = timestampUs;
    datagram.sumoVClass = std::move(vClass);
    return datagram;
}

VamInput PedestrianInput()
{
    VamInput input{};
    input.vruProfile = VruProfile::Pedestrian;
    input.vruRole = {
        VruRoleStatus::On,
        VbsState::ActiveStandalone
    };
    input.referencePosition.latitudeDeg = 0.0;
    input.referencePosition.longitudeDeg = 0.0;
    input.highFrequencyState.speedMps = 2.0;
    input.highFrequencyState.headingDeg = 90.0;
    return input;
}

VruRolePermissions TxAllowed()
{
    return {
        true,
        true,
        VruTxDecisionReason::ProfileAllowed,
        VruRxDecisionReason::RoleOn
    };
}

} // namespace

int main()
{
    auto& vehicles = VehicleRegistry::Instance();
    vehicles.Clear();

    SumoPersonRegistry persons;
    SumoVehicleLifecycleRegistry vehicleLifecycle;

    bool ok = true;
    std::string error;

    ok &= Check(
        persons.Apply(PersonUpsert(
            "ego-person", 1000000, 0.0, 0.0, 90.0, "pedestrian")),
        "FIXTURE_ACCEPTS_INITIAL_EGO_OBSERVATION");

    VamGenerationManager manager;
    VamProximityProvider provider;
    VamGenerationEvaluationAdapter adapter(manager, provider);

    VamGenerationEvaluationAdapterConfig config{};
    config.snapshotBuilder.vehicleThresholds = {2.0, 2.0, 1.0};
    config.snapshotBuilder.vruThresholds = {2.0, 2.0, 1.0};
    config.proximity.maximumSampleAgeUs = 200000;

    const VamInput input = PedestrianInput();
    const VruRolePermissions permissions = TxAllowed();

    VamGenerationContext context{};
    context.dccTGenVamMs = 150;

    // Establish a successfully confirmed baseline with the ego alone.
    VamGenerationResult initial{};
    const bool initialEvaluated = adapter.EvaluateForEgo(
        1000000,
        "ego-person",
        persons,
        vehicles,
        vehicleLifecycle,
        input,
        permissions,
        context,
        config,
        initial,
        error);

    ok &= Check(
        initialEvaluated &&
            error.empty() &&
            initial.outcome == VamGenerationOutcome::GenerationCandidate &&
            initial.reason == VamGenerationReason::InitialActivation &&
            initial.transactionId.has_value(),
        "INITIAL_EGO_EVALUATION_OPENS_GENERATION_CANDIDATE");

    bool initialConfirmed = false;
    if (initial.transactionId.has_value())
    {
        initialConfirmed = manager.ConfirmGeneration(
            *initial.transactionId, 1000, error);
    }

    ok &= Check(
        initialConfirmed,
        "TEST_FIXTURE_CONFIRMS_INITIAL_BASELINE_EXTERNALLY");

    // A new SUMO Person enters after the initial confirmed generation.
    ok &= Check(
        persons.Apply(PersonUpsert(
            "new-person", 1050000, 1.0, 0.0, 90.0, "bicycle")),
        "FIXTURE_ACCEPTS_NEW_PERSON_PRESENCE");

    ok &= Check(
        persons.Apply(PersonUpsert(
            "ego-person", 1150000, 0.0, 0.0, 90.0, "pedestrian")),
        "FIXTURE_ACCEPTS_LATEST_EGO_SAMPLE");

    ok &= Check(
        persons.Apply(PersonUpsert(
            "new-person", 1150000, 1.0, 0.0, 90.0, "bicycle")),
        "FIXTURE_ACCEPTS_LATEST_TARGET_SAMPLE");

    // The new person is inside all three strict thresholds:
    // lateral < 2 m, longitudinal < 2 m, vertical < 1 m.
    // The 150 ms DCC interval is also exactly satisfied.
    VamGenerationResult triggered{};
    error.clear();

    const bool evaluated = adapter.EvaluateForEgo(
        1150000,
        "ego-person",
        persons,
        vehicles,
        vehicleLifecycle,
        input,
        permissions,
        context,
        config,
        triggered,
        error);

    ok &= Check(
        evaluated && error.empty(),
        "ADAPTER_FORWARDS_PERCEPTION_TO_GENERATION_MANAGER");

    ok &= Check(
        triggered.outcome == VamGenerationOutcome::GenerationCandidate &&
            triggered.shouldGenerate &&
            triggered.reason ==
                VamGenerationReason::NewParticipantWithinSafeDistance &&
            triggered.transactionId.has_value(),
        "NEW_SUMO_PERSON_TRIGGERS_GENERATION_THROUGH_EVALUATE");

    ok &= Check(
        triggered.effectiveTGenVamMs == 150,
        "ADAPTER_PRESERVES_DCC_GENERATION_CONTEXT");

    // A second new person is first presented while the previous candidate
    // remains pending. The adapter must discard its staged provider history
    // when the manager rejects this evaluation.
    ok &= Check(
        persons.Apply(PersonUpsert(
            "pending-person", 1200000, 1.5, 0.0, 90.0, "bicycle")),
        "FIXTURE_ACCEPTS_PERSON_DURING_PENDING_TRANSACTION");

    ok &= Check(
        persons.Apply(PersonUpsert(
            "ego-person", 1200000, 0.0, 0.0, 90.0, "pedestrian")),
        "FIXTURE_ACCEPTS_EGO_SAMPLE_DURING_PENDING_TRANSACTION");

    VamGenerationResult pendingOutput{};
    pendingOutput.effectiveTGenVamMs = 777;
    error.clear();

    const bool evaluatedWhilePending = adapter.EvaluateForEgo(
        1200000,
        "ego-person",
        persons,
        vehicles,
        vehicleLifecycle,
        input,
        permissions,
        context,
        config,
        pendingOutput,
        error);

    ok &= Check(
        !evaluatedWhilePending &&
            error.find("transaction is still pending") != std::string::npos,
        "ADAPTER_REJECTS_EVALUATION_WHILE_GENERATION_IS_PENDING");

    ok &= Check(
        pendingOutput.effectiveTGenVamMs == 777,
        "FAILED_ADAPTER_EVALUATION_PRESERVES_CALLER_OUTPUT");

    VamProximitySnapshotSet retrySnapshots{};
    error.clear();

    const bool snapshotsBuilt = VamProximitySnapshotBuilder::BuildForEgo(
        "ego-person",
        persons,
        vehicles,
        vehicleLifecycle,
        config.snapshotBuilder,
        retrySnapshots,
        error);

    std::vector<VamSafeProximityObservation> retryObservations;
    const bool providerAccepted = snapshotsBuilt &&
        provider.BuildForEgo(
            retrySnapshots.ego,
            retrySnapshots.targets,
            1250000,
            config.proximity,
            retryObservations,
            error);

    ok &= Check(
        providerAccepted &&
            retryObservations.size() == 2 &&
            retryObservations[0].firstObservedAtMs == 1150 &&
            retryObservations[1].firstObservedAtMs == 1250,
        "REJECTED_GENERATION_EVALUATION_DOES_NOT_COMMIT_PROXIMITY_HISTORY");

    vehicles.Clear();

    if (!ok)
    {
        std::cout << "VAM_GENERATION_EVALUATION_ADAPTER=FAIL\n";
        return 1;
    }

    std::cout << "VAM_GENERATION_EVALUATION_ADAPTER=PASS\n";
    return 0;
}
