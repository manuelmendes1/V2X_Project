#pragma once

namespace ns3 {
struct ReplayContext;
}


#include "vanetza_integration/cam/types/cam_message.h"
#include "vanetza_integration/context/packet/packet_context.h"

namespace ns3 {

// ============================================================================
// ReplayProtection
// ----------------------------------------------------------------------------
// Proteção contra replay de mensagens CAM.
//
// O módulo utiliza exclusivamente:
//
//   • PacketContext
//   • CamMessage
//   • ReplayContext
//
// Não conhece VehicleState nem componentes Vanetza.
// ============================================================================

class ReplayProtection {

public:

    // ------------------------------------------------------------------------
    // Verifica e atualiza o estado de replay.
    //
    // true  -> mensagem aceite
    // false -> replay detetado
    // ------------------------------------------------------------------------

    static bool Process(
        PacketContext& ctx,
        const CamMessage& message,
        ReplayContext& replay);

private:

    static bool IsReplay(
        const PacketContext& ctx,
        const ReplayContext& replay,
        const CamMessage& message);

    static void Update(
        const PacketContext& ctx,
        ReplayContext& replay,
        const CamMessage& message);

    // ------------------------------------------------------------------------
    // Fingerprint da parte CAM do payload.
    //
    // A assinatura não participa no fingerprint.
    // ------------------------------------------------------------------------

    static uint64_t ComputeCamFingerprint(
        const PacketContext& ctx);
};

} // namespace ns3
