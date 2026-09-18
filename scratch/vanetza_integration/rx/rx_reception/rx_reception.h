#pragma once

#include "vanetza_integration/cam/decoder/cam_decoder.h"
#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_state.h"

class RxReception {

public:

    //=========================================================================
    // Executa a fase RECEPTION da pipeline RX.
    //
    //  - avança a pipeline para RECEPTION
    //  - entrega a CAM ao módulo de receção
    //
    // Em caso de sucesso devolve true.
    // Em caso de falha:
    //   • atualiza o PacketContext
    //   • escreve no logger
    //   • devolve false
    //=========================================================================
    static bool Process(ns3::PacketContext&ctx, const CamMessage&message, ns3::VehicleState&receiver, ns3::VehicleState&sender);
};
