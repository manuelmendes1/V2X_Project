#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "packet_types.h"

namespace ns3 {

// ============================================================================
// PacketIdManager
//
// Responsável pela geração de identificadores únicos dos pacotes V2X.
//
// Mantém três espaços de numeração independentes:
//
//  • packetId
//      Identificador global da simulação.
//
//  • vehiclePacketId
//      Contador monotónico por veículo.
//
//  • sequenceId
//      Contador monotónico por veículo e por tipo de mensagem.
//
// Exemplo:
//
// veh0
//      CAM
//          packetId        = 154
//          vehiclePacketId = 18
//          sequenceId      = 7
//
// veh1
//      CAM
//          packetId        = 155
//          vehiclePacketId = 7
//          sequenceId      = 7
//
// veh0
//      DENM
//          packetId        = 156
//          vehiclePacketId = 19
//          sequenceId      = 1
//
// ============================================================================

struct PayloadTypeHash {
    std::size_t operator()(PayloadType type) const noexcept {
        return static_cast<std::size_t>(type);
    }
};

class PacketIdManager {

public:

    static PacketIdManager& Instance();

    uint64_t NextPacketId();

    uint32_t NextVehiclePacketId(const std::string& vehId);

    uint16_t NextSequenceId(const std::string& vehId, PayloadType payloadType);

    void Reset();

private:

    using SequenceTable = std::unordered_map<PayloadType, uint16_t, PayloadTypeHash>;

    PacketIdManager() = default;

    //-------------------------------------------------------------------------
    // Global packet counter
    //-------------------------------------------------------------------------

    uint64_t m_nextPacketId = 1;

    //-------------------------------------------------------------------------
    // Packet counter per vehicle
    //-------------------------------------------------------------------------

    std::unordered_map<std::string, uint32_t> m_vehiclePacketCounters;

    //-------------------------------------------------------------------------
    // Sequence counter per vehicle and payload type
    //-------------------------------------------------------------------------

    std::unordered_map<std::string, SequenceTable> m_sequenceCounters;
};

} // namespace ns3
