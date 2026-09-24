#pragma once

#include <memory>
#include <vector>
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

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

    // Rotate the active DENM credential for this vehicle.
    // Returns false when the runtime is not initialized or when rotation
    // cannot be applied.
    bool RotateDenmCredential();

    void ReceivePacket(
        Ptr<Packet> packet,
        const Address& from);

    bool SendBtpB(
        PacketContext& context,
        const BtpTxRequest& request,
        const std::vector<uint8_t>& payload);

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
