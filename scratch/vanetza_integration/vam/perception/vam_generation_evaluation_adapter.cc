#include "vanetza_integration/vam/perception/vam_generation_evaluation_adapter.h"

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace vanetza_integration
{
namespace vam
{

VamGenerationEvaluationAdapter::VamGenerationEvaluationAdapter(
    VamGenerationManager& generationManager,
    VamProximityProvider& proximityProvider) noexcept
    : m_generationManager(generationManager),
      m_proximityProvider(proximityProvider)
{
}

bool
VamGenerationEvaluationAdapter::EvaluateForEgo(
    std::uint64_t referenceTimeUs,
    const std::string& egoPersonId,
    const ns3::SumoPersonRegistry& persons,
    const ns3::VehicleRegistry& vehicles,
    const ns3::SumoVehicleLifecycleRegistry& vehicleLifecycle,
    const VamInput& input,
    const vru::role::VruRolePermissions& permissions,
    const VamGenerationContext& context,
    const VamGenerationEvaluationAdapterConfig& config,
    VamGenerationResult& output,
    std::string& error)
{
    const std::uint64_t referenceTimeMs = referenceTimeUs / 1000;

    if (referenceTimeMs >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()))
    {
        error = "VAM reference time exceeds int64 milliseconds";
        return false;
    }

    VamProximitySnapshotSet snapshots{};
    if (!VamProximitySnapshotBuilder::BuildForEgo(
            egoPersonId,
            persons,
            vehicles,
            vehicleLifecycle,
            config.snapshotBuilder,
            snapshots,
            error))
    {
        return false;
    }

    // Stage perception history separately. A rejected manager evaluation
    // must not consume a first-observation timestamp.
    VamProximityProvider candidateProvider = m_proximityProvider;

    std::vector<VamSafeProximityObservation> observations;
    if (!candidateProvider.BuildForEgo(
            snapshots.ego,
            snapshots.targets,
            referenceTimeUs,
            config.proximity,
            observations,
            error))
    {
        return false;
    }

    // Preserve DCC, trajectory and redundancy inputs. Only the proximity
    // observations are supplied by the SUMO perception pipeline.
    VamGenerationContext evaluationContext = context;
    evaluationContext.safeProximityObservations = std::move(observations);

    // Evaluate opens a transaction for GenerationCandidate. Commit the
    // staged perception history only when the manager accepts the evaluation.
    // Confirmation or abortion remains the transmission layer's responsibility.
    if (!m_generationManager.Evaluate(
            static_cast<std::int64_t>(referenceTimeMs),
            input,
            permissions,
            evaluationContext,
            output,
            error))
    {
        return false;
    }

    m_proximityProvider = std::move(candidateProvider);
    return true;
}

} // namespace vam
} // namespace vanetza_integration
