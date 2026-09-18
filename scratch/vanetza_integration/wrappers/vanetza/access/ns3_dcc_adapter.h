#pragma once

#include "vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h"
#include "vanetza_integration/wrappers/vanetza/dcc/ns3_channel_load_probe.h"

#include "ns3/nstime.h"

#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/dcc/channel_load.hpp>
#include <vanetza/dcc/flow_control.hpp>
#include <vanetza/dcc/gradual_state_machine.hpp>
#include <vanetza/dcc/interface.hpp>
#include <vanetza/dcc/single_reactive_transmit_rate_control.hpp>

#include <ns3/event-id.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ns3
{

struct PacketContext;

class Ns3DccAdapter final : public vanetza::dcc::RequestInterface
{
public:
    Ns3DccAdapter(
        Ns3AccessAdapter& access,
        vanetza::ManualRuntime& runtime,
        std::string vehicleId,
        uint32_t nodeId,
        uint32_t stationId);

    ~Ns3DccAdapter() override;

    Ns3DccAdapter(const Ns3DccAdapter&) = delete;
    Ns3DccAdapter& operator=(const Ns3DccAdapter&) = delete;

    void request(
        const vanetza::dcc::DataRequest& request,
        std::unique_ptr<vanetza::ChunkPacket> packet) override;

private:
    using PacketContextPtr =
        std::shared_ptr<PacketContext>;

    void OnPacketTransmit(
        vanetza::access::AccessCategory category,
        const vanetza::ChunkPacket* packet);

    void OnPacketDrop(
        vanetza::access::AccessCategory category,
        const vanetza::ChunkPacket* packet);

    void ScheduleNextRuntimeEvent();

    PacketContextPtr FindContext(
        const vanetza::ChunkPacket* packet) const;

    void EraseContext(
        const vanetza::ChunkPacket* packet);

    void UpdateChannelLoad();

    void ScheduleNextChannelLoadMeasurement();

private:
    Ns3AccessAdapter& m_access;

    vanetza::ManualRuntime& m_runtime;

    // Diagnostic identity copied from the authoritative VehicleIdentity.
    // This is not an alternative identity authority.
    std::string m_vehicleId;
    uint32_t m_nodeId = 0;
    uint32_t m_stationId = 0;

    vanetza::dcc::GradualStateMachine m_stateMachine;

    vanetza::dcc::SingleReactiveTransmitRateControl m_trc;

    vanetza::dcc::FlowControl m_flowControl;

    std::unordered_map<
        const vanetza::ChunkPacket*,
        PacketContextPtr> m_packetContexts;

    EventId m_runtimeEvent;

    bool m_runtimeEventScheduled = false;

    vanetza::Clock::time_point m_runtimeEventDeadline{
        vanetza::Clock::time_point::max()};

    std::unique_ptr<Ns3ChannelLoadProbe>
        m_channelLoadProbe;

    EventId m_channelLoadEvent;

    bool m_channelLoadEventScheduled = false;
};

} // namespace ns3
