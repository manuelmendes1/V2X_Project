#include "packet_id_manager.h"

namespace ns3 {

// ============================================================================
// Singleton
// ============================================================================

PacketIdManager& PacketIdManager::Instance(){
    static PacketIdManager instance;

    return instance;
}

// ============================================================================
// Global packet identifier
// ============================================================================

uint64_t PacketIdManager::NextPacketId(){
    return m_nextPacketId++;
}

// ============================================================================
// Vehicle packet identifier
// ============================================================================

uint32_t PacketIdManager::NextVehiclePacketId(const std::string& vehId){
    return ++m_vehiclePacketCounters[vehId];
}

// ============================================================================
// ITS sequence identifier
// ============================================================================

uint16_t PacketIdManager::NextSequenceId(const std::string& vehId, PayloadType payloadType){
    return ++m_sequenceCounters[vehId][payloadType];
}

// ============================================================================
// Reset
// ============================================================================

void PacketIdManager::Reset(){
    m_nextPacketId = 1;

    m_vehiclePacketCounters.clear();

    m_sequenceCounters.clear();
}

} // namespace ns3
