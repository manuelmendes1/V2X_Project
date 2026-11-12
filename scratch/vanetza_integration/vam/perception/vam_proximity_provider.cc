#include "vanetza_integration/vam/perception/vam_proximity_provider.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace vanetza_integration
{
namespace vam
{
namespace
{

constexpr double kPi = 3.14159265358979323846;

bool Reject(std::string& error, const char* reason)
{
    error = reason;
    return false;
}

bool IsValidKinematics(const VamProximityKinematics& value)
{
    return std::isfinite(value.xMeters) &&
           std::isfinite(value.yMeters) &&
           std::isfinite(value.zMeters) &&
           std::isfinite(value.headingDeg);
}

bool IsValidThresholds(const VamProximityThresholds& value)
{
    // Zero thresholds cannot satisfy the strict proximity inequalities.
    // Require positive, finite policy values at the provider boundary.
    return std::isfinite(value.minimumSafeLateralDistanceMeters) &&
           std::isfinite(value.minimumSafeLongitudinalDistanceMeters) &&
           std::isfinite(value.minimumSafeVerticalDistanceMeters) &&
           value.minimumSafeLateralDistanceMeters > 0.0 &&
           value.minimumSafeLongitudinalDistanceMeters > 0.0 &&
           value.minimumSafeVerticalDistanceMeters > 0.0;
}

bool IsFresh(
    std::uint64_t sampleTimeUs,
    std::uint64_t referenceTimeUs,
    std::uint64_t maximumSampleAgeUs)
{
    if (sampleTimeUs > referenceTimeUs)
    {
        return false;
    }

    return referenceTimeUs - sampleTimeUs <= maximumSampleAgeUs;
}

bool IsValidTargetKind(VamProximityTargetKind kind)
{
    return kind == VamProximityTargetKind::Vehicle ||
           kind == VamProximityTargetKind::Vru;
}

bool IsValidPresenceEpoch(
    std::uint64_t presenceEpochUs,
    std::uint64_t sampleTimeUs)
{
    return presenceEpochUs <= sampleTimeUs;
}

struct ProjectedDistances
{
    double lateralMeters{};
    double longitudinalMeters{};
    double verticalMeters{};
};

ProjectedDistances CalculateDistances(
    const VamProximityKinematics& ego,
    const VamProximityKinematics& target)
{
    const double dx = target.xMeters - ego.xMeters;
    const double dy = target.yMeters - ego.yMeters;
    const double dz = target.zMeters - ego.zMeters;

    // SUMO navigation heading: 0 degrees North, 90 degrees East,
    // increasing clockwise. The target heading defines the local
    // longitudinal/lateral reference frame in this project model.
    const double normalizedHeading =
        std::fmod(target.headingDeg, 360.0);

    const double headingRad =
        normalizedHeading * kPi / 180.0;

    const double sinHeading = std::sin(headingRad);
    const double cosHeading = std::cos(headingRad);

    return {
        std::abs(dx * cosHeading - dy * sinHeading),
        std::abs(dx * sinHeading + dy * cosHeading),
        std::abs(dz)
    };
}

} // namespace

bool
VamProximityProvider::BuildForEgo(
    const VamProximityEgoSnapshot& ego,
    const std::vector<VamProximityTargetSnapshot>& targets,
    std::uint64_t referenceTimeUs,
    const VamProximityProviderConfig& config,
    std::vector<VamSafeProximityObservation>& output,
    std::string& error)
{
    if (ego.personId.empty())
    {
        return Reject(error, "Ego person ID must not be empty");
    }

    if (!IsValidKinematics(ego.kinematics))
    {
        return Reject(error, "Ego kinematics contain non-finite values");
    }

    if (!IsValidPresenceEpoch(
            ego.presenceEpochUs,
            ego.kinematics.sampleTimeUs))
    {
        return Reject(error, "Ego presence epoch follows its sample time");
    }

    if (referenceTimeUs /
            1000 >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()))
    {
        return Reject(error, "Reference time exceeds int64 milliseconds");
    }

    // A stale or future ego sample cannot support a perception evaluation.
    // It must not alter existing pair histories.
    if (!IsFresh(
            ego.kinematics.sampleTimeUs,
            referenceTimeUs,
            config.maximumSampleAgeUs))
    {
        output.clear();
        error.clear();
        return true;
    }

    const auto referenceTimeMs =
        static_cast<std::int64_t>(referenceTimeUs / 1000);

    // Work on a copy so every rejected build leaves provider state and
    // caller output unchanged.
    auto nextHistory = m_history;

    // A new ego presence period invalidates all previous pair histories.
    for (auto it = nextHistory.begin(); it != nextHistory.end();)
    {
        const auto& key = it->first;
        const auto& history = it->second;

        if (std::get<0>(key) == ego.personId &&
            history.egoPresenceEpochUs != ego.presenceEpochUs)
        {
            it = nextHistory.erase(it);
        }
        else
        {
            ++it;
        }
    }

    std::set<PairKey> presentPairs;
    std::set<PairKey> uniquePairs;

    std::vector<VamSafeProximityObservation> nextOutput;
    nextOutput.reserve(targets.size());

    for (const auto& target : targets)
    {
        if (target.participantId.empty())
        {
            return Reject(error, "Target participant ID must not be empty");
        }

        if (!IsValidTargetKind(target.kind))
        {
            return Reject(error, "Target kind is invalid");
        }

        const PairKey key{
            ego.personId,
            target.kind,
            target.participantId};

        if (!uniquePairs.insert(key).second)
        {
            return Reject(error, "Duplicate ego-target pair in input");
        }

        if (!IsValidKinematics(target.kinematics))
        {
            return Reject(
                error,
                "Target kinematics contain non-finite values");
        }

        if (!IsValidPresenceEpoch(
                target.presenceEpochUs,
                target.kinematics.sampleTimeUs))
        {
            return Reject(
                error,
                "Target presence epoch follows its sample time");
        }

        if (!IsValidThresholds(target.thresholds))
        {
            return Reject(
                error,
                "Target safe-proximity thresholds must be finite and positive");
        }

        // A person must not perceive itself as another VRU participant.
        if (target.kind == VamProximityTargetKind::Vru &&
            target.participantId == ego.personId)
        {
            continue;
        }

        // Presence in the complete input snapshot is retained even when
        // its current mobility sample is stale. A temporary stale sample
        // must not erase an established detection history.
        presentPairs.insert(key);

        if (!IsFresh(
                target.kinematics.sampleTimeUs,
                referenceTimeUs,
                config.maximumSampleAgeUs))
        {
            continue;
        }

        auto found = nextHistory.find(key);

        const bool newPresence =
            found == nextHistory.end() ||
            found->second.egoPresenceEpochUs != ego.presenceEpochUs ||
            found->second.targetPresenceEpochUs != target.presenceEpochUs;

        std::int64_t firstObservedAtMs = referenceTimeMs;

        if (!newPresence)
        {
            firstObservedAtMs =
                found->second.firstObservedAtMs;
        }

        nextHistory.insert_or_assign(
            key,
            PairHistory{
                ego.presenceEpochUs,
                target.presenceEpochUs,
                firstObservedAtMs});

        const auto distances =
            CalculateDistances(ego.kinematics, target.kinematics);

        // Finite coordinates do not guarantee finite differences or
        // projections: subtraction and rotation may overflow.
        // Reject before committing the candidate history or output.
        if (!std::isfinite(distances.lateralMeters) ||
            !std::isfinite(distances.longitudinalMeters) ||
            !std::isfinite(distances.verticalMeters))
        {
            return Reject(
                error,
                "Projected proximity distances contain non-finite values");
        }

        VamSafeProximityObservation observation{};
        observation.firstObservedAtMs = firstObservedAtMs;
        observation.lateralDistanceMeters =
            distances.lateralMeters;
        observation.longitudinalDistanceMeters =
            distances.longitudinalMeters;
        observation.verticalDistanceMeters =
            distances.verticalMeters;

        observation.minimumSafeLateralDistanceMeters =
            target.thresholds.minimumSafeLateralDistanceMeters;
        observation.minimumSafeLongitudinalDistanceMeters =
            target.thresholds.minimumSafeLongitudinalDistanceMeters;
        observation.minimumSafeVerticalDistanceMeters =
            target.thresholds.minimumSafeVerticalDistanceMeters;

        nextOutput.push_back(observation);
    }

    // Only pairs absent from the complete active-target snapshot lose
    // their history. Stale-but-present targets remain represented above.
    for (auto it = nextHistory.begin(); it != nextHistory.end();)
    {
        const auto& key = it->first;

        if (std::get<0>(key) == ego.personId &&
            presentPairs.find(key) == presentPairs.end())
        {
            it = nextHistory.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_history = std::move(nextHistory);
    output = std::move(nextOutput);
    error.clear();
    return true;
}

void
VamProximityProvider::ForgetEgo(
    const std::string& egoPersonId)
{
    for (auto it = m_history.begin(); it != m_history.end();)
    {
        if (std::get<0>(it->first) == egoPersonId)
        {
            it = m_history.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void
VamProximityProvider::Clear() noexcept
{
    m_history.clear();
}

} // namespace vam
} // namespace vanetza_integration
