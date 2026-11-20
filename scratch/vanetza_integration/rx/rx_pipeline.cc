#include "rx_pipeline.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/security/replay/replay_protection.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/rx/rx_context/rx_context.h"
#include "vanetza_integration/rx/rx_decode/rx_decode.h"
#include "vanetza_integration/rx/rx_reception/rx_reception.h"
#include "vanetza_integration/rx/rx_unpack/rx_unpack.h"
#include "vanetza_integration/rx/rx_validation/rx_validation.h"
#include "vanetza_integration/rx/rx_verify/rx_verify.h"

#include <ns3/simulator.h>

using namespace ns3;

void
RxPipeline::ProcessDirect(
PacketContext& ctx,
const std::vector<uint8_t>& buffer)
{
CamPayloadParts frame;
CamMessage message;
VehicleState* sender = nullptr;
VehicleState* receiver = nullptr;


//----------------------------------------------------------------------
// 3/9 GEONET_RX
//
// RAW_RX and ACCESS_RX were already completed by
// VanetzaRuntime::ReceivePacket() using this same PacketContext.
//----------------------------------------------------------------------
ctx.AdvancePipeline(
    PipelineStage::GEONET_RX);

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "GEONET",
    "GEONET_RX_BEGIN",
    "processing GeoNetworking indication");

ctx.MarkSuccess();

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "GEONET",
    "GEONET_RX_OK",
    "lat=" +
        std::to_string(ctx.geonet.latitude) +
        " lon=" +
        std::to_string(ctx.geonet.longitude));

//----------------------------------------------------------------------
// 4/9 BTP_RX
//----------------------------------------------------------------------
ctx.AdvancePipeline(
    PipelineStage::BTP_RX);

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "BTP",
    "BTP_RX_BEGIN",
    "port=" +
        std::to_string(ctx.btpDestinationPort));

if (!ExtractRaw(
        ctx,
        buffer,
        frame))
{
    return;
}

ctx.MarkSuccess();

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "BTP",
    "BTP_RX_OK",
    "port=" +
        std::to_string(
            ctx.btpDestinationPort) +
    " payload=" +
        std::to_string(
            frame.payload.size()) +
    " signature=" +
        std::to_string(
            frame.signature.size()));

//----------------------------------------------------------------------
// Resolve and validate endpoints.
//
// RxContext is the single authority for establishing and validating
// endpoint identity in PacketContext.
//
// RxPipeline does not reconstruct or overwrite sender/receiver
// identity fields.
//----------------------------------------------------------------------
if (!ResolveEndpoints(
        ctx,
        sender,
        receiver))
{
    return;
}


V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "RX",
    "CTX_READY",
    "sender=" +
        ctx.senderVehId +
    " sender_station=" +
        std::to_string(
            ctx.senderStationId) +
    " receiver=" +
        ctx.receiverVehId +
    " receiver_station=" +
        std::to_string(
            ctx.receiverStationId));

//----------------------------------------------------------------------
// 5/9 VERIFY
//----------------------------------------------------------------------
ctx.AdvancePipeline(
    PipelineStage::VERIFY);

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "SECURITY",
    "VERIFY_BEGIN");

if (!Verify(
        ctx,
        frame.payload,
        frame.signature,
        *sender,
        *receiver))
{
    return;
}

ctx.MarkSuccess();

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "SECURITY",
    "VERIFY_OK",
    "signature verified");

//----------------------------------------------------------------------
// 6/9 DECODE
//----------------------------------------------------------------------
ctx.AdvancePipeline(
    PipelineStage::DECODE);

if (!Decode(
        ctx,
        frame.payload,
        message))
{
    return;
}

// Bind the CAM semantic stationId to the identity established by the
// authenticated GeoNetworking receive boundary.
if (message.stationId == 0 ||
    message.stationId != ctx.senderStationId)
{
    ctx.MarkDrop();
    ctx.reason = "cam_sender_station_id_mismatch";

    V2XWriteLog(
        ctx,
        V2XLogLevel::WARN,
        "SECURITY",
        "CAM_STATION_ID_BINDING_FAIL",
        "payload_station=" + std::to_string(message.stationId) +
        " authenticated_sender_station=" +
            std::to_string(ctx.senderStationId));
    return;
}

if (!Validate(
        ctx,
        *sender))
{
    return;
}

ctx.MarkSuccess();

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "CAM",
    "DECODE_OK",
    "station=" +
        std::to_string(
            message.stationId) +
    " speed=" +
        std::to_string(
            message.speed));

//----------------------------------------------------------------------
// 7/9 REPLAY
//----------------------------------------------------------------------
ctx.AdvancePipeline(
    PipelineStage::REPLAY);

if (!Replay(
        ctx,
        message,
        *receiver))
{
    return;
}

ctx.MarkSuccess();

V2XWriteLog(
    ctx,
    V2XLogLevel::INFO,
    "REPLAY",
    "REPLAY_OK",
    "station=" +
        std::to_string(
            message.stationId) +
    " gdt=" +
        std::to_string(
            message.generationDeltaTime));

//----------------------------------------------------------------------
// 8/9 RECEPTION
//----------------------------------------------------------------------
ctx.AdvancePipeline(
    PipelineStage::RECEPTION);

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "CAM_RX",
    "RECEPTION_BEGIN");

if (!Reception(
        ctx,
        message,
        *sender,
        *receiver))
{
    return;
}

ctx.MarkSuccess();

V2XWriteLog(
    ctx,
    V2XLogLevel::INFO,
    "CAM_RX",
    "RECEPTION_OK",
    "sender=" +
        sender->identity.vehId +
    " sender_node=" +
        std::to_string(
            sender->identity.nodeId) +
    " sender_station=" +
        std::to_string(
            sender->identity.stationId) +
    " receiver=" +
        receiver->identity.vehId +
    " receiver_node=" +
        std::to_string(
            receiver->identity.nodeId) +
    " receiver_station=" +
        std::to_string(
            receiver->identity.stationId) +
    " pkt=" +
        std::to_string(
            ctx.packetId) +
    " vpkt=" +
        std::to_string(
            ctx.vehiclePacketId) +
    " seq=" +
        std::to_string(
            ctx.sequenceId) +
    " speed=" +
        std::to_string(
            message.speed) +
    " pos=(" +
        std::to_string(
            message.latitude) +
    "," +
        std::to_string(
            message.longitude) +
    ")");

//----------------------------------------------------------------------
// 9/9 DELIVER
//----------------------------------------------------------------------
ctx.AdvancePipeline(
    PipelineStage::DELIVER);

V2XWriteLog(
    ctx,
    V2XLogLevel::DEBUG,
    "RX",
    "DELIVER_BEGIN");

ctx.MarkSuccess();

V2XWriteLog(
    ctx,
    V2XLogLevel::INFO,
    "RX",
    "DELIVER_OK",
    "sender=" +
        sender->identity.vehId +
    " sender_station=" +
        std::to_string(
            sender->identity.stationId) +
    " pkt=" +
        std::to_string(
            ctx.packetId) +
    " vpkt=" +
        std::to_string(
            ctx.vehiclePacketId) +
    " seq=" +
        std::to_string(
            ctx.sequenceId));


}

bool RxPipeline::ExtractRaw(PacketContext& ctx, const std::vector<uint8_t>& buffer, CamPayloadParts& frame) {
    return RxUnpack::ExtractRaw(ctx, buffer, frame);
}

bool RxPipeline::ResolveEndpoints(
    PacketContext& ctx,
    VehicleState*& sender,
    VehicleState*& receiver)
{
    return RxContext::Resolve(ctx, sender, receiver);
}

bool RxPipeline::Decode(PacketContext& ctx, const std::vector<uint8_t>& payload, CamMessage& message) {
    return RxDecode::Process(ctx, payload, message);
}

bool RxPipeline::Validate(PacketContext& ctx, VehicleState& sender) {
    return RxValidation::Validate(ctx, sender);
}

bool RxPipeline::Verify(PacketContext& ctx, const std::vector<uint8_t>& payload, const std::vector<uint8_t>& signature, VehicleState& sender, VehicleState& receiver) {
    return RxVerify::Process(ctx, payload, signature, sender, receiver);
}

bool RxPipeline::Replay(PacketContext& ctx, const CamMessage& message, VehicleState& receiver) {
    return ReplayProtection::Process(ctx, message, receiver.security.replay);
}

bool RxPipeline::Reception(PacketContext& ctx, const CamMessage& message, VehicleState& sender, VehicleState& receiver) {
    return RxReception::Process(ctx, message, receiver, sender);
}
