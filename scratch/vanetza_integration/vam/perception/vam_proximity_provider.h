#pragma once

#include "vanetza_integration/vam/generation/vam_generation_manager.h"

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace vanetza_integration
{
namespace vam
{

enum class VamProximityTargetKind
{
    Vehicle,
    Vru
};

struct VamProximityKinematics
{
    double xMeters{};
    double yMeters{};
    double zMeters{};
    double headingDeg{};
    std::uint64_t sampleTimeUs{};
};

struct VamProximityThresholds
{
    double minimumSafeLateralDistanceMeters{};
    double minimumSafeLongitudinalDistanceMeters{};
    double minimumSafeVerticalDistanceMeters{};
};

struct VamProximityEgoSnapshot
{
    std::string personId;
    std::uint64_t presenceEpochUs{};
    VamProximityKinematics kinematics{};
};

struct VamProximityTargetSnapshot
{
    std::string participantId;
    VamProximityTargetKind kind{
        VamProximityTargetKind::Vehicle};

    std::uint64_t presenceEpochUs{};
    VamProximityKinematics kinematics{};
    VamProximityThresholds thresholds{};
};

struct VamProximityProviderConfig
{
    // Maximum accepted age of a SUMO mobility sample.
    // Zero permits only a sample at exactly referenceTimeUs.
    std::uint64_t maximumSampleAgeUs{};
};

class VamProximityProvider
{
public:
    bool BuildForEgo(
        const VamProximityEgoSnapshot& ego,
        const std::vector<VamProximityTargetSnapshot>& targets,
        std::uint64_t referenceTimeUs,
        const VamProximityProviderConfig& config,
        std::vector<VamSafeProximityObservation>& output,
        std::string& error);

    // Remove all detection histories owned by an ego person.
    // Call when the ego person leaves the active SUMO population.
    void ForgetEgo(const std::string& egoPersonId);

    void Clear() noexcept;

private:
    using PairKey = std::tuple<
        std::string,
        VamProximityTargetKind,
        std::string>;

    struct PairHistory
    {
        std::uint64_t egoPresenceEpochUs{};
        std::uint64_t targetPresenceEpochUs{};
        std::int64_t firstObservedAtMs{};
    };

    std::map<PairKey, PairHistory> m_history;
};

} // namespace vam
} // namespace vanetza_integration
