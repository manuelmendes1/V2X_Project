#include "rx_verify.h"

#include "vanetza_integration/security/cam_security.h"

bool RxVerify::Process(ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, const std::vector<uint8_t>& signature, const ns3::VehicleState& sender, const ns3::VehicleState& receiver) {
    const bool verified = CamSecurity::Verify(ctx, payload, signature, sender.security.certificate);

    if (!verified) {
        ctx.MarkFailure();
        ctx.reason = "signature_verification_failed";
        return false;
    }

    return true;
}
