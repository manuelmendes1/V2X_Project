#pragma once

#include <cstdint>
#include <string>

#include "packet_payload.h"
#include "packet_types.h"

#include "vanetza_integration/rx/rx_context/geonet_rx_context.h"

namespace ns3 {

// ============================================================================
// PacketContext
//
// Contexto único associado a um pacote V2X.
//
// O PacketContext acompanha um pacote durante todo o seu ciclo de vida,
// constituindo a fonte única de informação para:
//
//   • logging
//   • tracing
//   • profiling
//   • auditoria
//   • correlação entre módulos
//
// Fluxo TX:
//
//   GENERATE -> ENCODE -> SIGN -> BTP_TX -> GEONET_TX -> ACCESS_TX -> SEND -> TX_COMPLETE
//
// Fluxo RX:
//
//   RAW_RX -> GEONET_RX -> BTP_RX -> VERIFY -> DECODE -> REPLAY -> RECEPTION -> DELIVER
//
// Durante TX:
//
//   sender == receiver
//
// Durante RX:
//
//   sender   = veículo que originou o pacote
//   receiver = veículo que recebeu o pacote
// ============================================================================

struct PacketContext
{
    //=========================================================================
    // Simulation context
    //=========================================================================

    int64_t simulationStep = 0;
    double simulationTime = 0.0;

    // Timestamp temporal imutável do PacketContext,
    // expresso em microssegundos do relógio de simulação ns-3.
    uint64_t timestamp = 0;

    //=========================================================================
    // Sender identity
    //=========================================================================

    std::string senderVehId;

    uint32_t senderNodeId = 0;
    uint32_t senderStationId = 0;

    //=========================================================================
    // Receiver identity
    //=========================================================================

    std::string receiverVehId;

    uint32_t receiverNodeId = 0;
    uint32_t receiverStationId = 0;

    //=========================================================================
    // Packet identity
    //=========================================================================
    //
    // Estes identificadores pertencem sempre ao emissor.
    //=========================================================================

    uint64_t packetId = 0;
    uint32_t vehiclePacketId = 0;
    uint16_t sequenceId = 0;

    //=========================================================================
    // Pipeline state
    //=========================================================================
    //
    // pipelineStep e pipelineTotal são calculados automaticamente a partir
    // da etapa (PipelineStage).
    //=========================================================================

    uint16_t pipelineStep = 0;
    uint16_t pipelineTotal = 0;

    PipelineStage pipelineStage = PipelineStage::NONE;
    PipelineResult pipelineResult = PipelineResult::NONE;

    //=========================================================================
    // Execution metadata
    //=========================================================================

    std::string reason;

    uint64_t durationUs = 0;

    //=========================================================================
    // Payload
    //=========================================================================

    PacketPayload payload;

    //=========================================================================
    // BTP RX context
    //=========================================================================

    uint16_t btpDestinationPort = 0;

    //=========================================================================
    // GeoNetworking context
    //=========================================================================
    //
    // Informação original proveniente da camada GeoNetworking.
    // É preenchida na receção e acompanha o pacote durante toda a RX pipeline.
    //=========================================================================

    GeoNetRxContext geonet;

    //=========================================================================
    // Pipeline control
    //=========================================================================

    /*
     * Avança para uma nova etapa da pipeline.
     *
     * O PacketContext determina automaticamente:
     *
     *   • pipelineStep
     *   • pipelineTotal
     *   • pipelineResult = BEGIN
     */
    void AdvancePipeline(PipelineStage stage);

    //=========================================================================
    // Pipeline result
    //=========================================================================

    void MarkSuccess();

    void MarkFailure();

    void MarkDrop();

    void MarkResult(PipelineResult result);

    //=========================================================================
    // Execution helpers
    //=========================================================================

    /*
     * Limpa apenas informação transitória da execução,
     * preservando a identidade do pacote.
     */
    void ClearExecutionState();

    //=========================================================================
    // Context helpers
    //=========================================================================

    bool HasSender() const;

    bool HasReceiver() const;

    bool HasPayload() const;

    bool IsTx() const;

    bool IsRx() const;

    //=========================================================================
    // Validation
    //=========================================================================

    /*
     * Indica se o contexto representa um pacote válido.
     */
    bool IsValid() const;

    /*
     * Indica se o contexto está completamente vazio.
     */
    bool IsEmpty() const;

    //=========================================================================
    // Debug
    //=========================================================================

    /*
     * Representação textual resumida do contexto.
     */
    std::string ToString() const;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    /*
     * Repõe o contexto ao estado inicial.
     */
    void Reset();

private:

    /*
     * Obtém o índice lógico da etapa da pipeline.
     */
    static uint16_t GetPipelineStep(PipelineStage stage);

    /*
     * Obtém o número total de etapas da pipeline correspondente.
     */
    static uint16_t GetPipelineTotal(PipelineStage stage);
};

} // namespace ns3
