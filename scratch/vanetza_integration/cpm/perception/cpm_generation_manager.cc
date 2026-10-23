#include "vanetza_integration/cpm/perception/cpm_generation_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr std::uint32_t kProfileGenerationIntervalMinMs = 100;
constexpr std::uint32_t kProfileGenerationIntervalMaxMs = 1000;
// ETSI TS 103 324 V2.1.1, clause 7.1.8.6: the rating is in [0, 15].
constexpr std::uint8_t kMaxPerceptionQuality = 15;
constexpr std::size_t kMaxKnownObjects = 255;

bool Reject(std::string& error, const std::string& reason)
{
    error = reason;
    return false;
}

bool IsValidObjectType(CpmGenerationObjectType type)
{
    switch (type)
    {
    case CpmGenerationObjectType::TypeA:
    case CpmGenerationObjectType::TypeB:
        return true;
    }
    return false;
}

bool IsValidConfig(
    const CpmGenerationConfig& config,
    std::string& error)
{
    if (config.generationIntervalMs < kProfileGenerationIntervalMinMs ||
        config.generationIntervalMs > kProfileGenerationIntervalMaxMs)
    {
        return Reject(
            error,
            "CPM generation interval must be within 100..1000 ms");
    }

    if (config.maximumGenerationIntervalMs <
            kProfileGenerationIntervalMinMs ||
        config.maximumGenerationIntervalMs >
            kProfileGenerationIntervalMaxMs ||
        config.generationIntervalMs >
            config.maximumGenerationIntervalMs)
    {
        return Reject(
            error,
            "CPM maximum generation interval is invalid");
    }

    if (config.objectPerceptionQualityThreshold > kMaxPerceptionQuality)
    {
        return Reject(
            error,
            "CPM perception quality threshold must be within 0..15");
    }

    if (!std::isfinite(config.minPositionChangeThresholdMeters) ||
        config.minPositionChangeThresholdMeters < 0.0)
    {
        return Reject(error, "CPM position threshold is invalid");
    }

    if (!std::isfinite(
            config.minGroundSpeedChangeThresholdMetersPerSecond) ||
        config.minGroundSpeedChangeThresholdMetersPerSecond < 0.0)
    {
        return Reject(error, "CPM ground-speed threshold is invalid");
    }

    if (!std::isfinite(
            config.minGroundVelocityOrientationChangeThresholdDegrees) ||
        config.minGroundVelocityOrientationChangeThresholdDegrees < 0.0 ||
        config.minGroundVelocityOrientationChangeThresholdDegrees > 180.0)
    {
        return Reject(error, "CPM orientation threshold is invalid");
    }

    if (config.maxObjectsPerMessage == 0 ||
        config.maxObjectsPerMessage > kMaxKnownObjects)
    {
        return Reject(
            error,
            "CPM maximum object count must be within 1..255");
    }

    if (config.estimatedMessageOverheadBytes == 0 ||
        config.maxEstimatedPayloadBytes <=
            config.estimatedMessageOverheadBytes)
    {
        return Reject(
            error,
            "CPM estimated payload budget must exceed message overhead");
    }

    if (config.objectRetentionMs <= 0)
    {
        return Reject(error, "CPM object retention must be positive");
    }

    error.clear();
    return true;
}

bool IsValidCandidate(
    const CpmGenerationCandidate& candidate,
    std::string& error)
{
    if (candidate.objectId == 0)
    {
        return Reject(error, "CPM candidate objectId must be non-zero");
    }

    if (!IsValidObjectType(candidate.type))
    {
        return Reject(error, "CPM candidate object type is invalid");
    }

    if (!std::isfinite(candidate.xEastMeters) ||
        !std::isfinite(candidate.yNorthMeters))
    {
        return Reject(error, "CPM candidate position is not finite");
    }

    if (!std::isfinite(candidate.groundSpeedMetersPerSecond) ||
        candidate.groundSpeedMetersPerSecond < 0.0)
    {
        return Reject(error, "CPM candidate ground speed is invalid");
    }

    if (!std::isfinite(candidate.groundVelocityOrientationDeg) ||
        candidate.groundVelocityOrientationDeg < 0.0 ||
        candidate.groundVelocityOrientationDeg >= 360.0)
    {
        return Reject(
            error,
            "CPM candidate ground-velocity orientation must be in [0, 360)");
    }

    if (candidate.objectPerceptionQuality.has_value() &&
        *candidate.objectPerceptionQuality > kMaxPerceptionQuality)
    {
        return Reject(
            error,
            "CPM candidate perception quality must be within 0..15");
    }

    if (candidate.estimatedPayloadBytes == 0)
    {
        return Reject(
            error,
            "CPM candidate estimated payload size must be positive");
    }

    error.clear();
    return true;
}

bool MeetsQualityThreshold(
    const CpmGenerationCandidate& candidate,
    const CpmGenerationConfig& config)
{
    return candidate.objectPerceptionQuality.has_value() &&
           *candidate.objectPerceptionQuality >
               config.objectPerceptionQualityThreshold;
}

bool ElapsedAtLeast(
    std::int64_t nowMs,
    std::int64_t earlierMs,
    std::int64_t intervalMs)
{
    return nowMs >= earlierMs &&
           nowMs - earlierMs >= intervalMs;
}

double OrientationDifferenceDegrees(double lhs, double rhs)
{
    const double raw = std::fmod(std::abs(lhs - rhs), 360.0);
    return std::min(raw, 360.0 - raw);
}

CpmGenerationDecision MakeDecision(
    std::uint16_t objectId,
    bool selected,
    CpmGenerationDecisionReason reason)
{
    return CpmGenerationDecision{objectId, selected, reason};
}

} // namespace

CpmGenerationManager::CpmGenerationManager(CpmGenerationConfig config)
    : m_config(std::move(config))
{
}

bool
CpmGenerationManager::Evaluate(
    std::int64_t nowMs,
    const std::vector<CpmGenerationCandidate>& candidates,
    CpmGenerationResult& output,
    std::string& error)
{
    if (m_pendingTransaction.has_value())
    {
        return Reject(
            error,
            "CPM message-creation transaction is still pending");
    }

    // Todas as validações de fronteira antecedem a criação do estado candidato.
    if (!IsValidConfig(m_config, error))
    {
        return false;
    }

    if (nowMs < 0)
    {
        return Reject(error, "CPM logical time must be non-negative");
    }

    if (m_lastEvaluationAtMs.has_value() &&
        nowMs < *m_lastEvaluationAtMs)
    {
        return Reject(error, "CPM logical time regressed");
    }

    if (candidates.size() > kMaxKnownObjects)
    {
        return Reject(
            error,
            "Known object count exceeds the CPM model range");
    }

    std::set<std::uint16_t> seenIds;
    for (const auto& candidate : candidates)
    {
        if (!IsValidCandidate(candidate, error))
        {
            return false;
        }

        if (!seenIds.insert(candidate.objectId).second)
        {
            return Reject(
                error,
                "Duplicate CPM candidate objectId: " +
                    std::to_string(candidate.objectId));
        }
    }

    // Cópias locais garantem que falhas de validação não contaminam o estado.
    auto nextTracks = m_tracks;
    auto nextLastGenerationEventAtMs = m_lastGenerationEventAtMs;

    // Expirar tracks ausentes antes de incorporar a observação atual.
    for (auto it = nextTracks.begin(); it != nextTracks.end();)
    {
        const bool expired =
            nowMs >= it->second.lastSeenAtMs &&
            nowMs - it->second.lastSeenAtMs >= m_config.objectRetentionMs;

        if (expired)
        {
            it = nextTracks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Ordenar os candidatos por ID torna a seleção sob limites reproduzível,
    // independentemente da ordem da fonte.
    auto orderedCandidates = candidates;
    std::sort(
        orderedCandidates.begin(),
        orderedCandidates.end(),
        [](const CpmGenerationCandidate& lhs,
           const CpmGenerationCandidate& rhs) {
            return lhs.objectId < rhs.objectId;
        });

    for (const auto& candidate : orderedCandidates)
    {
        auto found = nextTracks.find(candidate.objectId);

        if (found == nextTracks.end())
        {
            TrackState state{};
            state.firstDetectedAtMs = nowMs;
            state.lastSeenAtMs = nowMs;
            nextTracks.emplace(candidate.objectId, std::move(state));
        }
        else
        {
            found->second.lastSeenAtMs = nowMs;
        }
    }

    CpmGenerationResult nextOutput{};
    std::vector<CpmGenerationCandidate> nextIncludedCandidates;
    nextIncludedCandidates.reserve(m_config.maxObjectsPerMessage);
    nextOutput.knownObjectCount = candidates.size();

    const bool generationEventDue =
        !nextLastGenerationEventAtMs.has_value() ||
        ElapsedAtLeast(
            nowMs,
            *nextLastGenerationEventAtMs,
            static_cast<std::int64_t>(m_config.generationIntervalMs));

    nextOutput.generationEventDue = generationEventDue;

    if (!generationEventDue)
    {
        nextOutput.outcome = CpmGenerationOutcome::IntervalNotElapsed;
        m_tracks = std::move(nextTracks);
        m_lastEvaluationAtMs = nowMs;
        output = std::move(nextOutput);
        error.clear();
        return true;
    }

    // ETSI TS 103 324 V2.1.1, clause 6.1.2.3:
    // quando um Type-A elegível ultrapassa metade de T_GenCpmMax desde
    // a sua última inclusão, os Type-A elegíveis atuais são incluídos em grupo.
    bool typeABatchRefresh = false;
    const auto typeABatchIntervalMs =
        static_cast<std::int64_t>(
            m_config.maximumGenerationIntervalMs / 2);

    for (const auto& candidate : orderedCandidates)
    {
        if (candidate.type != CpmGenerationObjectType::TypeA ||
            !MeetsQualityThreshold(candidate, m_config))
        {
            continue;
        }

        const auto& state = nextTracks.at(candidate.objectId);

        if (state.lastIncludedAtMs.has_value() &&
            ElapsedAtLeast(
                nowMs,
                *state.lastIncludedAtMs,
                typeABatchIntervalMs))
        {
            typeABatchRefresh = true;
            break;
        }
    }

    for (const auto& candidate : orderedCandidates)
    {
        auto& state = nextTracks.at(candidate.objectId);

        CpmGenerationDecisionReason reason =
            CpmGenerationDecisionReason::NoInclusionCriteria;

        if (!MeetsQualityThreshold(candidate, m_config))
        {
            nextOutput.decisions.push_back(MakeDecision(
                candidate.objectId,
                false,
                CpmGenerationDecisionReason::
                    BelowPerceptionQualityThreshold));
            continue;
        }

        const bool firstDetectionSinceLastEvent =
            !nextLastGenerationEventAtMs.has_value() ||
            state.firstDetectedAtMs > *nextLastGenerationEventAtMs;

        if (!state.lastIncludedAtMs.has_value())
        {
            reason = firstDetectionSinceLastEvent
                ? CpmGenerationDecisionReason::FirstDetection
                : CpmGenerationDecisionReason::FirstEligibleInclusion;
        }
        else if (firstDetectionSinceLastEvent)
        {
            reason = CpmGenerationDecisionReason::FirstDetection;
        }
        else if (state.lastIncludedCandidate.has_value() &&
                 candidate.type != state.lastIncludedCandidate->type)
        {
            reason = CpmGenerationDecisionReason::ObjectTypeChanged;
        }
        else if (candidate.type == CpmGenerationObjectType::TypeA &&
                 typeABatchRefresh)
        {
            reason = CpmGenerationDecisionReason::TypeABatchRefresh;
        }
        else if (candidate.type == CpmGenerationObjectType::TypeB &&
                 state.lastIncludedCandidate.has_value())
        {
            const auto& previous = *state.lastIncludedCandidate;

            const double positionChange = std::hypot(
                candidate.xEastMeters - previous.xEastMeters,
                candidate.yNorthMeters - previous.yNorthMeters);

            const double speedChange = std::abs(
                candidate.groundSpeedMetersPerSecond -
                previous.groundSpeedMetersPerSecond);

            const double orientationChange = OrientationDifferenceDegrees(
                candidate.groundVelocityOrientationDeg,
                previous.groundVelocityOrientationDeg);

            if (positionChange > m_config.minPositionChangeThresholdMeters)
            {
                reason = CpmGenerationDecisionReason::PositionChanged;
            }
            else if (speedChange >
                     m_config.minGroundSpeedChangeThresholdMetersPerSecond)
            {
                reason = CpmGenerationDecisionReason::GroundSpeedChanged;
            }
            else if (orientationChange >=
                     m_config
                         .minGroundVelocityOrientationChangeThresholdDegrees)
            {
                reason =
                    CpmGenerationDecisionReason::
                        GroundVelocityOrientationChanged;
            }
            else if (ElapsedAtLeast(
                         nowMs,
                         *state.lastIncludedAtMs,
                         static_cast<std::int64_t>(
                             m_config.maximumGenerationIntervalMs)))
            {
                reason =
                    CpmGenerationDecisionReason::MaximumInclusionInterval;
            }
        }

        if (reason == CpmGenerationDecisionReason::NoInclusionCriteria)
        {
            nextOutput.decisions.push_back(MakeDecision(
                candidate.objectId, false, reason));
            continue;
        }

        if (nextOutput.includedObjectIds.size() >=
            m_config.maxObjectsPerMessage)
        {
            nextOutput.decisions.push_back(MakeDecision(
                candidate.objectId,
                false,
                CpmGenerationDecisionReason::MaximumObjectCount));
            continue;
        }

        const std::size_t currentEstimatedSize =
            nextOutput.includedObjectIds.empty()
                ? m_config.estimatedMessageOverheadBytes
                : nextOutput.estimatedPayloadBytes;

        // Subtração após validação do orçamento evita overflow em adições.
        if (currentEstimatedSize > m_config.maxEstimatedPayloadBytes ||
            candidate.estimatedPayloadBytes >
                m_config.maxEstimatedPayloadBytes - currentEstimatedSize)
        {
            nextOutput.decisions.push_back(MakeDecision(
                candidate.objectId,
                false,
                CpmGenerationDecisionReason::PayloadBudgetExceeded));
            continue;
        }

        if (nextOutput.includedObjectIds.empty())
        {
            nextOutput.estimatedPayloadBytes =
                m_config.estimatedMessageOverheadBytes;
        }

        nextOutput.estimatedPayloadBytes += candidate.estimatedPayloadBytes;
        nextOutput.includedObjectIds.push_back(candidate.objectId);
        nextOutput.decisions.push_back(MakeDecision(
            candidate.objectId, true, reason));
        nextIncludedCandidates.push_back(candidate);
    }

    nextOutput.shouldCreateMessage =
        !nextOutput.includedObjectIds.empty();

    nextOutput.outcome = nextOutput.shouldCreateMessage
        ? CpmGenerationOutcome::MessageCandidateSetProduced
        : CpmGenerationOutcome::EventDueNoEligibleObjects;

    nextLastGenerationEventAtMs = nowMs;

    if (nextOutput.shouldCreateMessage)
    {
        if (m_nextTransactionId == 0)
        {
            return Reject(error, "CPM transaction identifier space exhausted");
        }

        PendingTransaction pending{};
        pending.transactionId = m_nextTransactionId;
        pending.evaluatedAtMs = nowMs;
        pending.tracks = std::move(nextTracks);
        pending.includedCandidates = std::move(nextIncludedCandidates);
        nextOutput.transactionId = pending.transactionId;

        // Nenhum estado de inclusão é confirmado nesta fase.
        m_pendingTransaction = std::move(pending);
        ++m_nextTransactionId;
    }
    else
    {
        // Um evento sem mensagem não exige confirmação de montagem.
        m_tracks = std::move(nextTracks);
        m_lastGenerationEventAtMs = nextLastGenerationEventAtMs;
        m_lastEvaluationAtMs = nowMs;
    }

    output = std::move(nextOutput);
    error.clear();
    return true;
}

bool
CpmGenerationManager::ConfirmMessageCreated(
    std::uint64_t transactionId,
    std::string& error)
{
    if (!m_pendingTransaction.has_value())
    {
        return Reject(error, "No CPM message-creation transaction is pending");
    }

    if (transactionId == 0 ||
        transactionId != m_pendingTransaction->transactionId)
    {
        return Reject(error, "CPM message-creation transaction ID mismatch");
    }

    // Preparar uma cópia permite validar o estado antes de o confirmar.
    auto committedTracks = m_pendingTransaction->tracks;

    for (const auto& candidate : m_pendingTransaction->includedCandidates)
    {
        const auto found = committedTracks.find(candidate.objectId);
        if (found == committedTracks.end())
        {
            return Reject(
                error,
                "Selected CPM object is missing from pending track state");
        }

        found->second.lastIncludedAtMs = m_pendingTransaction->evaluatedAtMs;
        found->second.lastIncludedCandidate = candidate;
    }

    const auto committedAtMs = m_pendingTransaction->evaluatedAtMs;
    m_tracks = std::move(committedTracks);
    m_lastGenerationEventAtMs = committedAtMs;
    m_lastEvaluationAtMs = committedAtMs;
    m_pendingTransaction.reset();

    error.clear();
    return true;
}

bool
CpmGenerationManager::AbortMessageCreation(
    std::uint64_t transactionId,
    std::string& error)
{
    if (!m_pendingTransaction.has_value())
    {
        return Reject(error, "No CPM message-creation transaction is pending");
    }

    if (transactionId == 0 ||
        transactionId != m_pendingTransaction->transactionId)
    {
        return Reject(error, "CPM message-creation transaction ID mismatch");
    }

    // O instante da avaliação foi aceite, mesmo que a criação da mensagem
    // tenha sido abortada. Preservar a monotonicidade sem confirmar os tracks
    // provisórios nem avançar o instante do último evento de geração.
    const auto abortedAtMs = m_pendingTransaction->evaluatedAtMs;
    m_pendingTransaction.reset();
    m_lastEvaluationAtMs = abortedAtMs;

    error.clear();
    return true;
}
