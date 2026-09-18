#pragma once

#include <ns3/tag.h>
#include <ns3/type-id.h>
#include <ns3/tag-buffer.h>

#include <cstdint>

// ============================================================================
// PacketIdTag
//
// Tag utilizado apenas pelo simulador para transportar o identificador global
// do pacote ao longo da stack ns-3.
//
// Este identificador NÃO faz parte do protocolo CAM e não é serializado para
// a rede. É utilizado exclusivamente para correlação de logs TX ↔ RX.
// ============================================================================

class PacketIdTag : public ns3::Tag {

public:

    PacketIdTag() = default;

    explicit PacketIdTag(uint64_t packetId);

    //=========================================================================
    // ns-3 Tag interface
    //=========================================================================

    static ns3::TypeId GetTypeId();

    ns3::TypeId GetInstanceTypeId() const override;

    uint32_t GetSerializedSize() const override;

    void Serialize(ns3::TagBuffer buffer) const override;

    void Deserialize(ns3::TagBuffer buffer) override;

    void Print(std::ostream& os) const override;

    //=========================================================================
    // Packet identifier
    //=========================================================================

    void SetPacketId(uint64_t packetId);

    uint64_t GetPacketId() const;

private:

    uint64_t m_packetId = 0;
};
