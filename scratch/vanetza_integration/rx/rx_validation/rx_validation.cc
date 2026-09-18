#include "rx_validation.h"

#include "vanetza_integration/core/v2x_log.h"

// ============================================================================
// Public interface
// ============================================================================

bool RxValidation::Validate(ns3::PacketContext& ctx, const ns3::VehicleState& sender) {

    if (!ValidateSelfReception(ctx, sender)) {
        return false;
    }

    if (!ValidateCertificate(ctx, sender)) {
        return false;
    }

    return true;
}

// ============================================================================
// Self reception
// ============================================================================

bool RxValidation::ValidateSelfReception(ns3::PacketContext& ctx, const ns3::VehicleState& sender) {

    if (sender.identity.nodeId != ctx.receiverNodeId) {
        return true;
    }

    ctx.MarkDrop();
    ctx.reason = "self_reception";

    V2XWriteLog(ctx, V2XLogLevel::TRACE, "RX", "SELF_RECEPTION", "packet ignored");

    return false;
}

// ============================================================================
// Certificate validation
// ============================================================================

bool RxValidation::ValidateCertificate(ns3::PacketContext& ctx, const ns3::VehicleState& sender) {

    if (sender.security.certificate) {
        return true;
    }

    ctx.MarkFailure();
    ctx.reason = "missing_certificate";

    V2XWriteLog(ctx, V2XLogLevel::WARN, "SECURITY", "CERTIFICATE_MISSING", "certificate unavailable");

    return false;
}
