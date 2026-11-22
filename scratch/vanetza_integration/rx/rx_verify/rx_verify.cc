#include "rx_verify.h"

#include "vanetza_integration/security/cam_security.h"
#include "vanetza_integration/core/v2x_log.h"

bool RxVerify::Process(ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, const std::vector<uint8_t>& signature, const ns3::VehicleState& sender, const ns3::VehicleState& receiver) {
    // The GeoNetworking SecurityEntity must already have verified the
    // outer ETSI V3 envelope and recorded its result in PacketContext.
    // The application-level CAM signature is only an additional integrity
    // check; it is not a substitute for V3 certificate-chain validation.
    if (!ctx.geonet.hasItsAid ||
        ctx.geonet.itsAid != 36 ||
        !ctx.verificationPerformed ||
        !ctx.verificationSucceeded ||
        ctx.verifiedItsAid != 36)
    {
        ctx.MarkDrop();
        ctx.reason =
            !ctx.verificationPerformed
                ? "cam_v3_verification_missing"
                : (!ctx.verificationSucceeded
                    ? "cam_v3_verification_failed"
                    : "cam_v3_verified_aid_mismatch");

        V2XWriteLog(
            ctx,
            V2XLogLevel::WARN,
            "SECURITY",
            "CAM_V3_VERIFY_FAIL",
            ctx.reason);
        return false;
    }

    const bool verified = CamSecurity::Verify(ctx, payload, signature, sender.security.certificate);

    if (!verified) {
        ctx.MarkFailure();
        ctx.reason = "signature_verification_failed";
        return false;
    }

    return true;
}
