#pragma once

#include <cstdint>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_state.h"

//=============================================================================
// RxVerify
//
// Responsável pela etapa VERIFY da pipeline RX.
//
//  - avança a pipeline para VERIFY
//  - verifica a assinatura do CAM recebido
//  - atualiza o PacketContext
//  - regista os eventos da etapa
//
// Em caso de falha:
//   • atualiza o PacketContext
//   • regista o erro no logger
//   • devolve false
//=============================================================================

class RxVerify {

public:

    //=========================================================================
    // Executa a etapa VERIFY da pipeline RX.
    //
    // Entrada:
    //   - PacketContext
    //   - payload (bytes do CAM)
    //   - signature (bytes da assinatura)
    //   - VehicleState do emissor
    //   - VehicleState do recetor
    //
    // Saída:
    //   - true  -> assinatura válida
    //   - false -> falha na verificação
    //=========================================================================
    static bool Process(ns3::PacketContext& ctx, const std::vector<uint8_t>& payload, const std::vector<uint8_t>& signature, const ns3::VehicleState& sender, const ns3::VehicleState& receiver);
};
