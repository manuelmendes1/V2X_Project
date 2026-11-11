#include "vanetza_integration/vam/generation/vam_generation_manager.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace vanetza_integration
{
namespace vam
{
namespace
{

constexpr double kEarthRadiusMeters = 6371008.8;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kComparisonEpsilon = 1.0e-9;

bool Reject(std::string& error, const char* message)
{
    error = message;
    return false;
}

bool IsFinite(const std::optional<double>& value)
{
    return !value.has_value() || std::isfinite(*value);
}

bool IsValidPosition(const VamReferencePosition& position)
{
    if (position.latitudeDeg.has_value() != position.longitudeDeg.has_value())
    {
        return false;
    }

    if (position.latitudeDeg.has_value())
    {
        if (!std::isfinite(*position.latitudeDeg) ||
            !std::isfinite(*position.longitudeDeg) ||
            *position.latitudeDeg < -90.0 || *position.latitudeDeg > 90.0 ||
            *position.longitudeDeg < -180.0 || *position.longitudeDeg > 180.0)
        {
            return false;
        }
    }

    return IsFinite(position.altitudeMeters);
}

bool IsValidKinematics(const VamHighFrequencyState& state)
{
    if (!IsFinite(state.headingDeg) || !IsFinite(state.speedMps) ||
        !IsFinite(state.longitudinalAccelerationMps2))
    {
        return false;
    }

    if (state.headingDeg.has_value() &&
        (*state.headingDeg < 0.0 || *state.headingDeg > 360.0))
    {
        return false;
    }

    if (state.speedMps.has_value() && *state.speedMps < 0.0)
    {
        return false;
    }

    return true;
}

bool IsValidInput(const VamInput& input)
{
    return IsValidPosition(input.referencePosition) &&
           IsValidKinematics(input.highFrequencyState);
}

bool IsValidConfig(const VamGenerationConfig& config)
{
    return config.tGenVamMinMs > 0 &&
           config.tGenVamMaxMs >= config.tGenVamMinMs &&
           config.tGenVamLfMinMs > 0 &&
           config.tCheckVamGenMs > 0 &&
           config.tCheckVamGenMs <= config.tGenVamMinMs &&
           config.numSkipVamsForRedundancyMitigation >= 2 &&
           config.numSkipVamsForRedundancyMitigation <= 10 &&
           std::isfinite(config.minPositionChangeThresholdMeters) &&
           config.minPositionChangeThresholdMeters > 0.0 &&
           std::isfinite(config.minGroundSpeedChangeThresholdMps) &&
           config.minGroundSpeedChangeThresholdMps > 0.0 &&
           std::isfinite(config.minGroundVelocityOrientationChangeThresholdDegrees) &&
           config.minGroundVelocityOrientationChangeThresholdDegrees > 0.0 &&
           config.minGroundVelocityOrientationChangeThresholdDegrees <= 180.0 &&
           std::isfinite(config.trajectoryInterceptionProbabilityChangeThresholdPercentagePoints) &&
           config.trajectoryInterceptionProbabilityChangeThresholdPercentagePoints > 0.0 &&
           config.trajectoryInterceptionProbabilityChangeThresholdPercentagePoints <= 100.0;
}

bool IsValidContext(
    const VamGenerationContext& context,
    std::int64_t nowMs)
{
    for (const auto& observation : context.safeProximityObservations)
    {
        if (observation.firstObservedAtMs < 0 ||
            observation.firstObservedAtMs > nowMs ||
            !std::isfinite(observation.lateralDistanceMeters) ||
            !std::isfinite(observation.longitudinalDistanceMeters) ||
            !std::isfinite(observation.verticalDistanceMeters) ||
            !std::isfinite(observation.minimumSafeLateralDistanceMeters) ||
            !std::isfinite(observation.minimumSafeLongitudinalDistanceMeters) ||
            !std::isfinite(observation.minimumSafeVerticalDistanceMeters) ||
            observation.lateralDistanceMeters < 0.0 ||
            observation.longitudinalDistanceMeters < 0.0 ||
            observation.verticalDistanceMeters < 0.0 ||
            observation.minimumSafeLateralDistanceMeters < 0.0 ||
            observation.minimumSafeLongitudinalDistanceMeters < 0.0 ||
            observation.minimumSafeVerticalDistanceMeters < 0.0)
        {
            return false;
        }
    }

    if (context.trajectoryInterceptionProbabilityPercent.has_value())
    {
        const double probability =
            *context.trajectoryInterceptionProbabilityPercent;
        if (!std::isfinite(probability) || probability < 0.0 || probability > 100.0)
        {
            return false;
        }
    }

    if (context.redundancy.similarPeer.has_value())
    {
        const auto& peer = *context.redundancy.similarPeer;
        if (!IsValidPosition(peer.referencePosition) ||
            !IsValidKinematics(peer.highFrequencyState))
        {
            return false;
        }
    }

    return true;
}

std::uint32_t EffectiveInterval(
    const VamGenerationConfig& config,
    const VamGenerationContext& context)
{
    const auto requested = context.dccTGenVamMs.value_or(config.tGenVamMinMs);
    return std::clamp(requested, config.tGenVamMinMs, config.tGenVamMaxMs);
}

bool ElapsedAtLeast(
    std::int64_t nowMs,
    std::int64_t thenMs,
    std::uint32_t intervalMs)
{
    return nowMs >= thenMs &&
           static_cast<std::uint64_t>(nowMs - thenMs) >= intervalMs;
}

bool ElapsedStrictlyMoreThan(
    std::int64_t nowMs,
    std::int64_t thenMs,
    std::uint32_t intervalMs)
{
    return nowMs >= thenMs &&
           static_cast<std::uint64_t>(nowMs - thenMs) > intervalMs;
}

double DegreesToRadians(double degrees)
{
    return degrees * kPi / 180.0;
}

std::optional<double> PositionDistanceMeters(
    const VamReferencePosition& lhs,
    const VamReferencePosition& rhs)
{
    if (!lhs.latitudeDeg || !lhs.longitudeDeg ||
        !rhs.latitudeDeg || !rhs.longitudeDeg)
    {
        return std::nullopt;
    }

    const double lat1 = DegreesToRadians(*lhs.latitudeDeg);
    const double lat2 = DegreesToRadians(*rhs.latitudeDeg);
    const double deltaLat = lat2 - lat1;
    const double deltaLon = DegreesToRadians(
        std::remainder(*rhs.longitudeDeg - *lhs.longitudeDeg, 360.0));

    const double sinLat = std::sin(deltaLat / 2.0);
    const double sinLon = std::sin(deltaLon / 2.0);
    const double a = sinLat * sinLat +
        std::cos(lat1) * std::cos(lat2) * sinLon * sinLon;
    const double boundedA = std::clamp(a, 0.0, 1.0);
    return 2.0 * kEarthRadiusMeters *
           std::atan2(std::sqrt(boundedA), std::sqrt(1.0 - boundedA));
}

double CircularDifferenceDegrees(double lhs, double rhs)
{
    const double difference = std::fmod(std::abs(lhs - rhs), 360.0);
    return std::min(difference, 360.0 - difference);
}

bool IsValidStandaloneTx(
    const VamInput& input,
    const vru::role::VruRolePermissions& permissions)
{
    return permissions.transmit &&
           input.vruRole.role == vru::role::VruRoleStatus::On &&
           input.vruRole.vbs == vru::role::VbsState::ActiveStandalone;
}

std::optional<VamGenerationReason> DetermineTrigger(
    std::int64_t nowMs,
    std::int64_t lastGenerationAtMs,
    const VamGenerationConfig& config,
    const VamInput& input,
    const VamGenerationContext& context,
    const VamReferencePosition& previousPosition,
    const VamHighFrequencyState& previousKinematics,
    const std::optional<double>& previousTrajectoryProbability,
    bool previousHadClusterOperation)
{
    if (ElapsedStrictlyMoreThan(
            nowMs, lastGenerationAtMs, config.tGenVamMaxMs))
    {
        return VamGenerationReason::MaximumGenerationInterval;
    }

    // A participant is new only if its first observation happened
    // strictly after the last successfully confirmed VAM transmission.
    // All three safe-distance conditions must hold simultaneously.
    for (const auto& observation : context.safeProximityObservations)
    {
        if (observation.firstObservedAtMs > lastGenerationAtMs &&
            observation.lateralDistanceMeters <
                observation.minimumSafeLateralDistanceMeters &&
            observation.longitudinalDistanceMeters <
                observation.minimumSafeLongitudinalDistanceMeters &&
            observation.verticalDistanceMeters <
                observation.minimumSafeVerticalDistanceMeters)
        {
            return VamGenerationReason::NewParticipantWithinSafeDistance;
        }
    }

    // A newly-present cluster operation must be announced in an individual VAM.
    // VamClusterOperation is currently a marker type, so presence transition is
    // the available semantic change until its operation fields are modelled.
    if (input.clusterOperation.has_value() && !previousHadClusterOperation)
    {
        return VamGenerationReason::ClusterOperationChanged;
    }

    const auto positionChange = PositionDistanceMeters(
        input.referencePosition, previousPosition);
    if (positionChange.has_value() &&
        *positionChange > config.minPositionChangeThresholdMeters +
                              kComparisonEpsilon)
    {
        return VamGenerationReason::PositionChanged;
    }

    const auto& currentKinematics = input.highFrequencyState;
    if (currentKinematics.speedMps.has_value() &&
        previousKinematics.speedMps.has_value() &&
        std::abs(*currentKinematics.speedMps - *previousKinematics.speedMps) >
            config.minGroundSpeedChangeThresholdMps + kComparisonEpsilon)
    {
        return VamGenerationReason::GroundSpeedChanged;
    }

    if (currentKinematics.headingDeg.has_value() &&
        previousKinematics.headingDeg.has_value() &&
        CircularDifferenceDegrees(
            *currentKinematics.headingDeg,
            *previousKinematics.headingDeg) >
            config.minGroundVelocityOrientationChangeThresholdDegrees +
                kComparisonEpsilon)
    {
        return VamGenerationReason::GroundVelocityOrientationChanged;
    }

    if (context.trajectoryInterceptionProbabilityPercent.has_value() &&
        previousTrajectoryProbability.has_value() &&
        std::abs(*context.trajectoryInterceptionProbabilityPercent -
                 *previousTrajectoryProbability) >
            config.trajectoryInterceptionProbabilityChangeThresholdPercentagePoints +
                kComparisonEpsilon)
    {
        return VamGenerationReason::TrajectoryInterceptionProbabilityChanged;
    }

    return std::nullopt;
}

bool HasPeerRedundancy(
    std::int64_t nowMs,
    std::int64_t lastGenerationAtMs,
    const VamGenerationConfig& config,
    const VamInput& input,
    const VamPeerObservation& peer)
{
    const std::uint64_t horizon =
        static_cast<std::uint64_t>(config.numSkipVamsForRedundancyMitigation) *
        static_cast<std::uint64_t>(config.tGenVamMaxMs);

    if (nowMs < lastGenerationAtMs ||
        static_cast<std::uint64_t>(nowMs - lastGenerationAtMs) > horizon)
    {
        return false;
    }

    const auto distance = PositionDistanceMeters(
        input.referencePosition, peer.referencePosition);
    if (!distance.has_value() ||
        !input.highFrequencyState.speedMps.has_value() ||
        !peer.highFrequencyState.speedMps.has_value() ||
        !input.highFrequencyState.headingDeg.has_value() ||
        !peer.highFrequencyState.headingDeg.has_value())
    {
        return false;
    }

    const bool positionSimilar =
        *distance < config.minPositionChangeThresholdMeters;
    const bool speedSimilar =
        std::abs(*input.highFrequencyState.speedMps -
                 *peer.highFrequencyState.speedMps) <
        config.minGroundSpeedChangeThresholdMps;
    const bool orientationSimilar =
        CircularDifferenceDegrees(*input.highFrequencyState.headingDeg,
                                  *peer.highFrequencyState.headingDeg) <
        config.minGroundVelocityOrientationChangeThresholdDegrees;

    return positionSimilar && speedSimilar && orientationSimilar;
}

bool ShouldSuppressForRedundancy(
    std::int64_t nowMs,
    std::int64_t lastGenerationAtMs,
    const VamGenerationConfig& config,
    const VamInput& input,
    const VamGenerationContext& context)
{
    const auto& redundancy = context.redundancy;

    if (redundancy.inProtectedOrNonDrivableArea ||
        redundancy.inPedestrianOnlyLowRiskArea ||
        redundancy.egoReportedByAnotherItsSWithinTGenVam)
    {
        return true;
    }

    return redundancy.similarPeer.has_value() &&
           HasPeerRedundancy(nowMs, lastGenerationAtMs, config, input,
                             *redundancy.similarPeer);
}

} // namespace

VamGenerationManager::VamGenerationManager(VamGenerationConfig config)
    : m_config(std::move(config))
{
}

bool VamGenerationManager::Evaluate(
    std::int64_t nowMs,
    const VamInput& input,
    const vru::role::VruRolePermissions& permissions,
    const VamGenerationContext& context,
    VamGenerationResult& output,
    std::string& error)
{
    if (m_pendingGeneration.has_value())
    {
        return Reject(error,
                      "VAM generation transaction is still pending");
    }
    if (!IsValidConfig(m_config))
    {
        return Reject(error, "Invalid VAM generation configuration");
    }
    if (nowMs < 0)
    {
        return Reject(error, "VAM logical time must be non-negative");
    }
    if (m_lastEvaluationAtMs.has_value() && nowMs < *m_lastEvaluationAtMs)
    {
        return Reject(error, "VAM logical time regressed");
    }
    if (!IsValidInput(input))
    {
        return Reject(error, "Invalid VAM input position or kinematics");
    }
    if (!IsValidContext(context, nowMs))
    {
        return Reject(error, "Invalid VAM generation context");
    }

    VamGenerationResult next{};
    next.effectiveTGenVamMs = EffectiveInterval(m_config, context);

    const bool txEligible = IsValidStandaloneTx(input, permissions);
    if (!txEligible)
    {
        next.outcome = VamGenerationOutcome::BlockedByPolicy;
        m_lastEvaluationAtMs = nowMs;
        m_lastEvaluationWasEligible = false;
        output = std::move(next);
        error.clear();
        return true;
    }

    const bool activation = !m_lastEvaluationWasEligible ||
                            !m_lastGenerationAtMs.has_value();

    // A geração inicial é imediata. Todas as gerações seguintes respeitam
    // o intervalo mínimo efetivamente fornecido pela gestão/DCC.
    if (m_lastGenerationAtMs.has_value() &&
        !ElapsedAtLeast(nowMs, *m_lastGenerationAtMs,
                        next.effectiveTGenVamMs))
    {
        next.outcome = VamGenerationOutcome::NotDue;
        m_lastEvaluationAtMs = nowMs;
        // Não consumir uma transição de ativação enquanto o intervalo
        // mínimo ainda impede a emissão correspondente.
        if (!activation)
        {
            m_lastEvaluationWasEligible = true;
        }
        output = std::move(next);
        error.clear();
        return true;
    }

    VamGenerationReason reason = VamGenerationReason::InitialActivation;
    bool triggered = activation;
    if (activation)
    {
        reason = VamGenerationReason::InitialActivation;
    }
    else
    {
        const auto trigger = DetermineTrigger(
            nowMs,
            *m_lastGenerationAtMs,
            m_config,
            input,
            context,
            m_lastConfirmedSnapshot->referencePosition,
            m_lastConfirmedSnapshot->highFrequencyState,
            m_lastConfirmedSnapshot->trajectoryInterceptionProbabilityPercent,
            m_lastConfirmedSnapshot->hasClusterOperation);
        triggered = trigger.has_value();
        if (trigger.has_value())
        {
            reason = *trigger;
        }
    }

    if (!triggered)
    {
        next.outcome = VamGenerationOutcome::NoTrigger;
        m_lastEvaluationAtMs = nowMs;
        m_lastEvaluationWasEligible = true;
        output = std::move(next);
        error.clear();
        return true;
    }

    if (m_lastGenerationAtMs.has_value() &&
        ShouldSuppressForRedundancy(nowMs, *m_lastGenerationAtMs,
                                    m_config, input, context) &&
        m_consecutiveRedundancySkips <
            m_config.numSkipVamsForRedundancyMitigation)
    {
        ++m_consecutiveRedundancySkips;
        next.outcome = VamGenerationOutcome::SuppressedByRedundancy;
        m_lastEvaluationAtMs = nowMs;
        m_lastEvaluationWasEligible = true;
        output = std::move(next);
        error.clear();
        return true;
    }

    if (m_nextTransactionId == 0)
    {
        return Reject(error, "VAM generation transaction identifier exhausted");
    }

    next.outcome = VamGenerationOutcome::GenerationCandidate;
    next.shouldGenerate = true;
    next.reason = reason;
    next.includeLowFrequencyContainer =
        !m_lastLowFrequencyContainerAtMs.has_value() ||
        ElapsedAtLeast(nowMs, *m_lastLowFrequencyContainerAtMs,
                       m_config.tGenVamLfMinMs);
    next.transactionId = m_nextTransactionId;

    PendingGeneration pending{};
    pending.transactionId = m_nextTransactionId;
    pending.evaluatedAtMs = nowMs;
    pending.includeLowFrequencyContainer = next.includeLowFrequencyContainer;
    pending.snapshot.referencePosition = input.referencePosition;
    pending.snapshot.highFrequencyState = input.highFrequencyState;
    pending.snapshot.trajectoryInterceptionProbabilityPercent =
        context.trajectoryInterceptionProbabilityPercent;
    pending.snapshot.hasClusterOperation = input.clusterOperation.has_value();
    m_pendingGeneration = std::move(pending);
    ++m_nextTransactionId;

    output = std::move(next);
    error.clear();
    return true;
}

bool VamGenerationManager::ConfirmGeneration(
    std::uint64_t transactionId,
    std::int64_t confirmedAtMs,
    std::string& error)
{
    if (!m_pendingGeneration.has_value())
    {
        return Reject(error, "No VAM generation transaction is pending");
    }
    if (transactionId == 0 ||
        transactionId != m_pendingGeneration->transactionId)
    {
        return Reject(error, "VAM generation transaction ID mismatch");
    }

    if (confirmedAtMs < m_pendingGeneration->evaluatedAtMs)
    {
        return Reject(
            error,
            "VAM confirmation time precedes evaluation time");
    }

    const auto committedAtMs = confirmedAtMs;
    const bool includedLf =
        m_pendingGeneration->includeLowFrequencyContainer;
    m_lastConfirmedSnapshot = m_pendingGeneration->snapshot;
    m_lastGenerationAtMs = committedAtMs;
    if (includedLf)
    {
        m_lastLowFrequencyContainerAtMs = committedAtMs;
    }
    m_lastEvaluationAtMs = committedAtMs;
    m_lastEvaluationWasEligible = true;
    m_consecutiveRedundancySkips = 0;
    m_pendingGeneration.reset();
    error.clear();
    return true;
}

bool VamGenerationManager::AbortGeneration(
    std::uint64_t transactionId,
    std::string& error)
{
    if (!m_pendingGeneration.has_value())
    {
        return Reject(error, "No VAM generation transaction is pending");
    }
    if (transactionId == 0 ||
        transactionId != m_pendingGeneration->transactionId)
    {
        return Reject(error, "VAM generation transaction ID mismatch");
    }

    const auto abortedAtMs = m_pendingGeneration->evaluatedAtMs;
    m_pendingGeneration.reset();
    m_lastEvaluationAtMs = abortedAtMs;
    error.clear();
    return true;
}

} // namespace vam
} // namespace vanetza_integration
