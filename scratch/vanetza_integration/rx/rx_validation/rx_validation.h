#pragma once

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_state.h"

class RxValidation {

public:

    //=========================================================================
    // Executa todas as validações da pipeline RX.
    //
    //  - self reception
    //  - certificado do emissor
    //
    // Em caso de falha:
    //   • atualiza o PacketContext
    //   • regista o evento no logger
    //   • devolve false
    //=========================================================================
    static bool Validate(ns3::PacketContext& ctx, const ns3::VehicleState& sender);

private:

    //=========================================================================
    // Ignora pacotes enviados pelo próprio veículo.
    //=========================================================================
    static bool ValidateSelfReception(ns3::PacketContext& ctx, const ns3::VehicleState& sender);

    //=========================================================================
    // Verifica se o emissor possui um certificado válido.
    //=========================================================================
    static bool ValidateCertificate(ns3::PacketContext& ctx, const ns3::VehicleState& sender);
};
