#include "rx_context.h"

#include "vanetza_integration/cam/types/cam_message.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/core/v2x_log.h"

#include <sstream>

// ============================================================================
// Resolve using PacketContext
// ============================================================================

bool RxContext::Resolve(
    ns3::PacketContext& ctx,
    ns3::VehicleState*& sender,
    ns3::VehicleState*& receiver)
{
    sender = ResolveSender(ctx);

    if (!sender)
        return false;

    if (!PopulateSenderContext(ctx, *sender))
        return false;

    receiver = ResolveReceiver(ctx);

    if (!receiver)
        return false;

    if (!PopulateReceiverContext(ctx, *receiver))
        return false;

    return true;
}

// ============================================================================
// Legacy path: resolve using CAM StationID
// ============================================================================

bool RxContext::Resolve(ns3::PacketContext& ctx, const CamMessage& message, ns3::VehicleState*& sender, ns3::VehicleState*& receiver) {

    sender = ResolveSender(ctx, message.stationId);

    if (!sender)
        return false;

    if (!PopulateSenderContext(ctx, *sender))
        return false;

    receiver = ResolveReceiver(ctx);

    if (!receiver)
        return false;

    if (!PopulateReceiverContext(ctx, *receiver))
        return false;

    return true;
}

// ============================================================================
// Resolve sender by StationID
// ============================================================================

ns3::VehicleState* RxContext::ResolveSender(ns3::PacketContext& ctx, uint32_t stationId) {

    auto* sender = ns3::VehicleRegistry::Instance().GetByStationId(stationId);

    V2XWriteLog(ctx, V2XLogLevel::DEBUG, "RX", "RESOLVE_SENDER_DEBUG", "stationId=" + std::to_string(stationId) + " -> sender=" + (sender ? sender->identity.vehId + "(station=" + std::to_string(sender->identity.stationId) + ")" : "NULL") + " ctx.receiverNodeId=" + std::to_string(ctx.receiverNodeId));

    if (!sender) {

        ctx.MarkFailure();
        ctx.reason = "station_not_found";

        V2XWriteLog(ctx, V2XLogLevel::WARN, "RX", "SENDER_NOT_FOUND", "station=" + std::to_string(stationId));

        return nullptr;
    }

    return sender;
}

// ============================================================================
// Resolve sender by GeoNetworking sender MAC
// ============================================================================

ns3::VehicleState* RxContext::ResolveSender(ns3::PacketContext& ctx) {

    const auto& linkMac = ctx.geonet.senderLinkMac;

    auto* sender = ns3::VehicleRegistry::Instance().GetByLinkMac(linkMac);

    if (!ctx.geonet.hasSenderLinkMac) {
        ctx.MarkFailure();
        ctx.reason = "sender_link_mac_missing";

        V2XWriteLog(ctx, V2XLogLevel::ERROR, "RX", "SENDER_NOT_FOUND", "sender Link MAC is not available");

        return nullptr;
    }

    return sender;
}

// ============================================================================
// Resolve receiver
// ============================================================================

ns3::VehicleState* RxContext::ResolveReceiver(ns3::PacketContext& ctx) {

    auto* receiver = ns3::VehicleRegistry::Instance().GetByNodeId(ctx.receiverNodeId);

    if (!receiver) {

        ctx.MarkFailure();
        ctx.reason = "receiver_not_found";

        V2XWriteLog(ctx, V2XLogLevel::ERROR, "RX", "RECEIVER_NOT_FOUND");

        return nullptr;
    }

    return receiver;
}

// ============================================================================
// Populate sender context
// ============================================================================

bool RxContext::PopulateSenderContext(
    ns3::PacketContext& ctx,
    const ns3::VehicleState& sender)
{
    const bool hasSenderContext =
        !ctx.senderVehId.empty() ||
        ctx.senderNodeId != 0 ||
        ctx.senderStationId != 0;

    if (hasSenderContext)
    {
        const bool matches =
            ctx.senderVehId == sender.identity.vehId &&
            ctx.senderNodeId == sender.identity.nodeId &&
            ctx.senderStationId == sender.identity.stationId;

        if (!matches)
        {
            ctx.MarkFailure();
            ctx.reason = "sender_context_mismatch";

            V2XWriteLog(
                ctx,
                V2XLogLevel::ERROR,
                "RX",
                "PACKET_CONTEXT_INTEGRITY_FAIL",
                "sender context does not match resolved sender");

            return false;
        }

        return true;
    }

    ctx.senderVehId = sender.identity.vehId;
    ctx.senderNodeId = sender.identity.nodeId;
    ctx.senderStationId = sender.identity.stationId;

    return true;
}

// ============================================================================
// Populate receiver context
// ============================================================================

bool RxContext::PopulateReceiverContext(
    ns3::PacketContext& ctx,
    const ns3::VehicleState& receiver)
{
    // receiverNodeId is a resolution key supplied by RxContextBuilder.
    // It does not by itself mean that the authoritative receiver identity
    // has already been established in PacketContext.
    const bool hasReceiverContext =
        !ctx.receiverVehId.empty() ||
        ctx.receiverStationId != 0;

    if (hasReceiverContext)
    {
        const bool matches =
            ctx.receiverVehId == receiver.identity.vehId &&
            ctx.receiverNodeId == receiver.identity.nodeId &&
            ctx.receiverStationId == receiver.identity.stationId;

        if (!matches)
        {
            ctx.MarkFailure();
            ctx.reason = "receiver_context_mismatch";

            V2XWriteLog(
                ctx,
                V2XLogLevel::ERROR,
                "RX",
                "PACKET_CONTEXT_INTEGRITY_FAIL",
                "receiver context does not match resolved receiver");

            return false;
        }

        return true;
    }

    ctx.receiverVehId = receiver.identity.vehId;
    ctx.receiverNodeId = receiver.identity.nodeId;
    ctx.receiverStationId = receiver.identity.stationId;

    return true;
}
