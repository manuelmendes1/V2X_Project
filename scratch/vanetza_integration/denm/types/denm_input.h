#pragma once

#include <cstdint>
#include <optional>

enum class DenmManagementAction : uint8_t
{
    NEW = 0,
    UPDATE,
    CANCELLATION,
    NEGATION
};

// ============================================================================
// DENM ActionID
//
// Identidade semântica da ação DENM.
//
// NÃO é:
//   - PacketContext::packetId
//   - PacketContext::vehiclePacketId
//   - PacketContext::sequenceId do CAM
//   - link MAC
//   - GN_ADDR
//
// O originatingStationId pertence ao ActionID ETSI.
// O sequenceNumber é um contador próprio do ciclo de vida DENM.
// ============================================================================

struct DenmActionId
{
    uint32_t originatingStationId = 0;
    uint16_t sequenceNumber = 0;
};

// ============================================================================
// Event position
// ============================================================================

struct DenmEventPosition
{
    double latitude = 0.0;
    double longitude = 0.0;
};

// ============================================================================
// Event cause
// ============================================================================

struct DenmCause
{
    uint16_t causeCode = 0;
    uint16_t subCauseCode = 0;
};

// ============================================================================
// Optional location information
//
// Estas grandezas permanecem em unidades físicas neutras.
// A conversão para as unidades/quantizações ETSI pertence ao codec.
// ============================================================================

struct DenmLocationData
{
    std::optional<double> eventSpeedMps;
    std::optional<double> eventHeadingDeg;
};

// ============================================================================
// DenmInput
//
// Modelo neutro utilizado pela aplicação para solicitar a geração de um DENM.
// Não contém tipos ASN.1 e não conhece Vanetza.
// ============================================================================

struct DenmInput
{
    // Tipo de ação sobre o evento.
    DenmManagementAction action = DenmManagementAction::NEW;

    // Identidade lógica da ação DENM.
    DenmActionId actionId;

    // TimestampIts em milissegundos, antes da codificação ASN.1.
    uint64_t detectionTimeItsMs = 0;
    uint64_t referenceTimeItsMs = 0;

    // Campo obrigatório da semântica DENM.
    DenmEventPosition eventPosition;

    // SituationContainer.
    uint8_t informationQuality = 0;
    DenmCause eventType;

    // StationType ETSI.
    uint32_t stationType = 0;

    // ManagementContainer opcionais.
    std::optional<uint32_t> validityDurationMs;
    std::optional<uint32_t> transmissionIntervalMs;

    // LocationContainer opcional.
    DenmLocationData location;
};
