#ifndef VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_GENERATION_MANAGER_H
#define VANETZA_INTEGRATION_CPM_PERCEPTION_CPM_GENERATION_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Categorias funcionais da regra de inclusão da ETSI TS 103 324 V2.1.1.
// A classificação deve ser fornecida pela fonte; este gestor não infere
// classes de objetos nem classifica veículos SUMO.
enum class CpmGenerationObjectType
{
    TypeA,
    TypeB
};

enum class CpmGenerationDecisionReason
{
    NoInclusionCriteria,
    FirstDetection,
    FirstEligibleInclusion,
    TypeABatchRefresh,
    PositionChanged,
    GroundSpeedChanged,
    GroundVelocityOrientationChanged,
    MaximumInclusionInterval,
    BelowPerceptionQualityThreshold,
    MaximumObjectCount,
    PayloadBudgetExceeded,
    ObjectTypeChanged
};

// Candidato neutro. estimatedPayloadBytes é uma estimativa fornecida
// pelo chamador, não o tamanho real do payload UPER.
struct CpmGenerationCandidate
{
    std::uint16_t objectId{};
    CpmGenerationObjectType type{CpmGenerationObjectType::TypeB};

    double xEastMeters{};
    double yNorthMeters{};
    double groundSpeedMetersPerSecond{};
    double groundVelocityOrientationDeg{};

    std::optional<std::uint8_t> objectPerceptionQuality;
    std::size_t estimatedPayloadBytes{50};
};

struct CpmGenerationConfig
{
    // Valores de referência para o perfil CPM R2 deste projeto.
    std::uint32_t generationIntervalMs{100};
    std::uint32_t maximumGenerationIntervalMs{1000};

    std::uint8_t objectPerceptionQualityThreshold{3};
    double minPositionChangeThresholdMeters{4.0};
    double minGroundSpeedChangeThresholdMetersPerSecond{0.5};
    double minGroundVelocityOrientationChangeThresholdDegrees{4.0};

    // Limites explícitos para este MVP de mensagem única.
    std::size_t maxObjectsPerMessage{8};
    std::size_t maxEstimatedPayloadBytes{1200};
    std::size_t estimatedMessageOverheadBytes{64};

    // Política local para remover do estado tracks que deixaram de ser vistos.
    // Não é um temporizador normativo da ETSI.
    std::int64_t objectRetentionMs{2000};
};

struct CpmGenerationDecision
{
    std::uint16_t objectId{};
    bool selected{};
    CpmGenerationDecisionReason reason{
        CpmGenerationDecisionReason::NoInclusionCriteria};
};

enum class CpmGenerationOutcome
{
    IntervalNotElapsed,
    EventDueNoEligibleObjects,
    // Conjunto candidato selecionado; não implica encoding ou transmissão.
    MessageCandidateSetProduced
};

struct CpmGenerationResult
{
    CpmGenerationOutcome outcome{
        CpmGenerationOutcome::IntervalNotElapsed};
    // Um evento periódico pode ocorrer sem produzir mensagem.
    bool generationEventDue{};
    bool shouldCreateMessage{};

    // ID atribuído à transação aberta por esta avaliação, se houver.
    // O valor fica na cópia do resultado mesmo depois de resolvida a transação.
    // Um token resolvido não pode ser reutilizado para confirmar ou abortar.
    // Não constitui prova de encoding ou transmissão.
    std::optional<std::uint64_t> transactionId;

    // Número de candidatos fornecidos nesta avaliação, não o número incluído.
    std::size_t knownObjectCount{};

    // Ordenados por objectId para resultados reprodutíveis.
    std::vector<std::uint16_t> includedObjectIds;
    std::vector<CpmGenerationDecision> decisions;

    // Inclui estimatedMessageOverheadBytes quando pelo menos um objeto
    // foi selecionado. Não substitui a medição do UPER real.
    std::size_t estimatedPayloadBytes{};
};

class CpmGenerationManager
{
public:
    explicit CpmGenerationManager(CpmGenerationConfig config = {});

    // Avalia os candidatos observados no instante lógico nowMs.
    //
    // - nowMs tem de ser não negativo e não regredir relativamente à
    //   avaliação anterior aceite;
    // - os eventos são limitados pelo intervalo configurado;
    // - uma seleção que produz mensagem abre uma transação pendente;
    // - a inclusão só é confirmada após ConfirmMessageCreated();
    // - AbortMessageCreation() descarta a avaliação pendente;
    // - avaliações adicionais são rejeitadas enquanto houver transação pendente;
    // - eventos sem objetos elegíveis não criam mensagem;
    // - erros preservam output e o estado confirmado.
    bool Evaluate(
        std::int64_t nowMs,
        const std::vector<CpmGenerationCandidate>& candidates,
        CpmGenerationResult& output,
        std::string& error);

    // Chamar após a montagem bem-sucedida de uma mensagem CPM.
    // Não significa que tenha ocorrido encoding ou transmissão.
    bool ConfirmMessageCreated(std::uint64_t transactionId, std::string& error);

    // Cancela a seleção pendente sem confirmar a inclusão dos objetos.
    bool AbortMessageCreation(std::uint64_t transactionId, std::string& error);

private:
    struct TrackState
    {
        std::int64_t firstDetectedAtMs{};
        std::int64_t lastSeenAtMs{};

        std::optional<std::int64_t> lastIncludedAtMs;
        std::optional<CpmGenerationCandidate> lastIncludedCandidate;
    };

    struct PendingTransaction
    {
        std::uint64_t transactionId{};
        std::int64_t evaluatedAtMs{};
        std::map<std::uint16_t, TrackState> tracks;
        std::vector<CpmGenerationCandidate> includedCandidates;
    };

    CpmGenerationConfig m_config;

    std::map<std::uint16_t, TrackState> m_tracks;
    std::optional<std::int64_t> m_lastGenerationEventAtMs;
    std::optional<std::int64_t> m_lastEvaluationAtMs;
    std::optional<PendingTransaction> m_pendingTransaction;
    std::uint64_t m_nextTransactionId{1};
};

#endif
