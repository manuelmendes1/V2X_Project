#pragma once

#include <memory>
#include <vector>

#include <ns3/packet.h>
#include <ns3/ptr.h>

namespace ns3
{

class VehicleState;
class VanetzaGeoNetAdapter;

struct PacketContext;

/**
 * @brief Runtime integration layer between ns-3 and the Vanetza wrapper.
 *
 * VanetzaRuntime is an orchestration layer.
 *
 * It owns:
 *
 *   - the authoritative VehicleState reference
 *   - PacketContext / RX bridge coordination
 *   - the VanetzaGeoNetAdapter
 *
 * All direct Vanetza protocol/runtime objects are owned by
 * VanetzaGeoNetAdapter.
 */
class VanetzaRuntime
{
public:

    explicit VanetzaRuntime(
        VehicleState& vehicle);

    ~VanetzaRuntime();

    VanetzaRuntime(
        const VanetzaRuntime&) = delete;

    VanetzaRuntime& operator=(
        const VanetzaRuntime&) = delete;

    VanetzaRuntime(
        VanetzaRuntime&&) = delete;

    VanetzaRuntime& operator=(
        VanetzaRuntime&&) = delete;

    bool Initialize();

    void Shutdown();

    bool IsInitialized() const;

    VehicleState& Vehicle();

    bool PrepareTx();

    void ReceivePacket(
        Ptr<Packet> packet,
        const Address& from);

    bool SendBtpB(
        PacketContext& context,
        uint16_t destinationPort,
        const std::vector<uint8_t>& payload);

private:

    VehicleState& m_vehicle;

    bool m_initialized = false;

    std::unique_ptr<VanetzaGeoNetAdapter>
        m_geonet;
};

} // namespace ns3
