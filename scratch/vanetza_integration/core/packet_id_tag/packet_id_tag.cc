#include "packet_id_tag.h"

#include <ns3/uinteger.h>

NS_OBJECT_ENSURE_REGISTERED(PacketIdTag);

// ============================================================================
// Constructors
// ============================================================================

PacketIdTag::PacketIdTag(uint64_t packetId) : m_packetId(packetId) {
}

// ============================================================================
// TypeId
// ============================================================================

ns3::TypeId PacketIdTag::GetTypeId() {

    static ns3::TypeId tid = ns3::TypeId("PacketIdTag").SetParent<ns3::Tag>().AddConstructor<PacketIdTag>();

    return tid;
}

ns3::TypeId PacketIdTag::GetInstanceTypeId() const {
    return GetTypeId();
}

// ============================================================================
// Serialization
// ============================================================================

uint32_t PacketIdTag::GetSerializedSize() const {
    return sizeof(uint64_t);
}

void PacketIdTag::Serialize(ns3::TagBuffer buffer) const {
    buffer.WriteU64(m_packetId);
}

void PacketIdTag::Deserialize(ns3::TagBuffer buffer) {
    m_packetId = buffer.ReadU64();
}

// ============================================================================
// Debug
// ============================================================================

void PacketIdTag::Print(std::ostream& os) const {
    os << "packetId=" << m_packetId;
}

// ============================================================================
// Accessors
// ============================================================================

void PacketIdTag::SetPacketId(uint64_t packetId) {
    m_packetId = packetId;
}

uint64_t PacketIdTag::GetPacketId() const {
    return m_packetId;
}
