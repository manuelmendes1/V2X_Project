#include "cam_builder.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"

#include "ns3/simulator.h"

#include <memory>

namespace ns3 {

    CamGenerationContext CamBuilder::Build(const VehicleState& vehicle) {

        CamGenerationContext ctx{};

        //=========================================================================
        // Packet Context
        //=========================================================================

        ctx.packetContext = std::make_shared<PacketContext>();

        PacketContext& pkt = *ctx.packetContext;

        //-------------------------------------------------------------------------
        // Identidade da simulação
        //-------------------------------------------------------------------------

        const auto now = Simulator::Now();

        pkt.simulationStep = SimulationContext::simulationStep;
        pkt.simulationTime = now.GetSeconds();

        // Timestamp temporal imutável do PacketContext.
        // A unidade é microssegundos do relógio de simulação ns-3.
        pkt.timestamp = now.GetMicroSeconds();

        //-------------------------------------------------------------------------
        // Identidade do pacote
        //-------------------------------------------------------------------------
        //
        // packetId / vehiclePacketId / sequenceId são atribuídos exclusivamente
        // pelo PacketIdManager em CamService::GenerateRawPayload().

        //-------------------------------------------------------------------------
        // Identidade do veículo
        //-------------------------------------------------------------------------

        pkt.senderVehId = vehicle.identity.vehId;
        pkt.senderNodeId = vehicle.identity.nodeId;
        pkt.senderStationId = vehicle.identity.stationId;

        //-------------------------------------------------------------------------
        // Estado inicial do contexto
        //
        // O CamBuilder apenas constrói o contexto. A pipeline é iniciada pelo
        // CamService através de PacketContext::AdvancePipeline().
        //-------------------------------------------------------------------------

        pkt.pipelineStep = 0;
        pkt.pipelineTotal = 0;
        pkt.pipelineStage = PipelineStage::NONE;
        pkt.pipelineResult = PipelineResult::NONE;

        pkt.reason.clear();
        pkt.durationUs = 0;

        //-------------------------------------------------------------------------
        // Payload
        //-------------------------------------------------------------------------

        pkt.payload.Clear();

        //=========================================================================
        // CAM Input
        //=========================================================================

        ctx.camInput.stationId = vehicle.identity.stationId;
        ctx.camInput.latitude = vehicle.mobility.position.x;
        ctx.camInput.longitude = vehicle.mobility.position.y;
        ctx.camInput.speed = vehicle.mobility.speed;
        ctx.camInput.heading = vehicle.mobility.heading;
        ctx.camInput.acceleration = vehicle.mobility.accel;

        //-------------------------------------------------------------------------
        // ETSI GenerationDeltaTime
        //
        // Tempo de geração da CAM em milissegundos (mod 65536),
        // conforme EN 302 637-2.
        //-------------------------------------------------------------------------

        const uint64_t nowMs = Simulator::Now().GetMilliSeconds();

        ctx.camInput.generationDeltaTime = static_cast<uint16_t>(nowMs % 65536);

        //=========================================================================
        // Security Context
        //=========================================================================

        ctx.security.privateKey = vehicle.security.privateKey;
        ctx.security.certificate = vehicle.security.certificate;

        return ctx;
    }

} // namespace ns3
