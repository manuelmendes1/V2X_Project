#pragma once

#include "vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h"
#include "vanetza_integration/wrappers/vanetza/dcc/ns3_channel_load_probe.h"
#include "vanetza_integration/context/tx_bridge/tx_packet_binding_registry.h"

#include "ns3/nstime.h"

#include <vanetza/common/clock.hpp>
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

namespace ns3
{

class Ns3DccAdapter final : public vanetza::dcc::RequestInterface
{
public:
    Ns3DccAdapter(
        Ns3AccessAdapter& access,
        vanetza::ManualRuntime& runtime,
        vanetza::Clock::time_point itsStartTime,
        std::string endpointId,
        uint32_t nodeId,
        uint32_t stationId,
        std::string identityLabel = "veh");

    ~Ns3DccAdapter() override;

    Ns3DccAdapter(const Ns3DccAdapter&) = delete;
    Ns3DccAdapter& operator=(const Ns3DccAdapter&) = delete;

    void request(
        const vanetza::dcc::DataRequest& request,
        std::unique_ptr<vanetza::ChunkPacket> packet) override;

    // Export the earliest Vanetza Runtime deadline to the ns-3 Simulator.
    // This is needed after Router operations that may create internal
    // Runtime callbacks, e.g. CBF forwarding timers.
    void SynchronizeRuntimeScheduler();

    // Advance ITS time before the Router constructs a TX packet.
    void SynchronizeRuntimeForTransmit();

private:
    void OnPacketTransmit(
        vanetza::access::AccessCategory category,
        const vanetza::ChunkPacket* packet);

    void OnPacketDrop(
        vanetza::access::AccessCategory category,
        const vanetza::ChunkPacket* packet);

    // Advance Vanetza's absolute ITS clock to the current ns-3 time.
    void AdvanceRuntimeToSimulatorNow();

    void ScheduleNextRuntimeEvent();

    void UpdateChannelLoad();

    void ScheduleNextChannelLoadMeasurement();

private:
    Ns3AccessAdapter& m_access;

    vanetza::ManualRuntime& m_runtime;

    // Absolute ITS clock corresponding to ns-3 simulation time zero.
    const vanetza::Clock::time_point m_itsStartTime;

    // Diagnostic label and identifier; neither is an identity authority.
    std::string m_endpointId;
    std::string m_identityLabel = "veh";
    uint32_t m_nodeId = 0;
    uint32_t m_stationId = 0;

    vanetza::dcc::GradualStateMachine m_stateMachine;

    vanetza::dcc::SingleReactiveTransmitRateControl m_trc;

    vanetza::dcc::FlowControl m_flowControl;

    // Preserves PacketContext and terminal completion together while DCC
    // holds a packet in its transmit queue.
    TxPacketBindingRegistry m_packetBindings;

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
