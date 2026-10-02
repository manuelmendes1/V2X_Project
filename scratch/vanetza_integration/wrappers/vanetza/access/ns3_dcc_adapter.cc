#include "ns3_dcc_adapter.h"

#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"

#include <ns3/simulator.h>
#include <ns3/wifi-phy.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

namespace ns3
{

Ns3DccAdapter::Ns3DccAdapter(
    Ns3AccessAdapter& access,
    vanetza::ManualRuntime& runtime,
    std::string vehicleId,
    uint32_t nodeId,
    uint32_t stationId)
    : m_access(access),
      m_runtime(runtime),
      m_vehicleId(std::move(vehicleId)),
      m_nodeId(nodeId),
      m_stationId(stationId),
      m_stateMachine(vanetza::dcc::etsiStates1ms),
      m_trc(m_stateMachine, m_runtime),
      m_flowControl(m_runtime, m_trc, m_access)
{
    m_flowControl.set_packet_transmit_hook(
        [this](
            vanetza::access::AccessCategory category,
            const vanetza::ChunkPacket* packet)
        {
            OnPacketTransmit(category, packet);
        });

    m_flowControl.set_packet_drop_hook(
        [this](
            vanetza::access::AccessCategory category,
            const vanetza::ChunkPacket* packet)
        {
            OnPacketDrop(category, packet);
        });

    // Unlimited queue. Packet lifetime remains the authoritative
    // expiration mechanism supplied by the DCC request.

    m_channelLoadProbe =
        std::make_unique<Ns3ChannelLoadProbe>(
            m_access.GetWifiPhy(),
            MilliSeconds(100));

    ScheduleNextChannelLoadMeasurement();
}

Ns3DccAdapter::~Ns3DccAdapter()
{
    if (m_runtimeEventScheduled)
    {
        m_runtimeEvent.Cancel();
        m_runtimeEventScheduled = false;
    }

    if (m_channelLoadEventScheduled)
    {
        m_channelLoadEvent.Cancel();
        m_channelLoadEventScheduled = false;
    }

    m_packetContexts.clear();
}

Ns3DccAdapter::PacketContextPtr
Ns3DccAdapter::FindContext(
    const vanetza::ChunkPacket* packet) const
{
    if (!packet)
    {
        return nullptr;
    }

    const auto it =
        m_packetContexts.find(packet);

    if (it == m_packetContexts.end())
    {
        return nullptr;
    }

    return it->second;
}

void
Ns3DccAdapter::EraseContext(
    const vanetza::ChunkPacket* packet)
{
    if (!packet)
    {
        return;
    }

    m_packetContexts.erase(packet);
}

void
Ns3DccAdapter::OnPacketTransmit(
    vanetza::access::AccessCategory category,
    const vanetza::ChunkPacket* packet)
{
    PacketContextPtr ctx =
        FindContext(packet);

    if (!ctx)
    {
        return;
    }

    /*
     * FlowControl invokes this hook immediately before forwarding the
     * packet to the access layer. Publish the same shared PacketContext
     * so Ns3AccessAdapter::request() can capture it safely.
     */
    TxBridgeContext::Push(ctx);

    /*
     * Record both the original application request time and the actual
     * ns-3 transmission time. This makes the DCC-induced delay directly
     * observable without modifying PacketContext::simulationTime.
     */
    const auto txSimUs =
        Simulator::Now().GetMicroSeconds();

    const auto requestSimUs =
        static_cast<int64_t>(
            ctx->simulationTime * 1'000'000.0);

    const auto dccDelayUs =
        txSimUs - requestSimUs;

    V2XWriteLog(
        *ctx,
        V2XLogLevel::DEBUG,
        "DCC",
        "DCC_TX",
        "access_category=" +
            std::to_string(
                static_cast<unsigned>(category)) +
        " tx_sim_us=" +
            std::to_string(txSimUs) +
        " dcc_delay_us=" +
            std::to_string(dccDelayUs));

    /*
     * Ns3AccessAdapter::request() captures the shared_ptr immediately
     * from TxBridgeContext and clears the thread-local bridge. The local
     * packet-context map can therefore release its reference here.
     */
    EraseContext(packet);
}

void
Ns3DccAdapter::OnPacketDrop(
    vanetza::access::AccessCategory category,
    const vanetza::ChunkPacket* packet)
{
    PacketContextPtr ctx =
        FindContext(packet);

    if (!ctx)
    {
        return;
    }

    const auto dropSimUs =
        Simulator::Now().GetMicroSeconds();

    const auto requestSimUs =
        static_cast<int64_t>(
            ctx->simulationTime * 1'000'000.0);

    const auto dccAgeUs =
        dropSimUs - requestSimUs;

    V2XWriteLog(
        *ctx,
        V2XLogLevel::WARN,
        "DCC",
        "DCC_DROP",
        "access_category=" +
            std::to_string(
                static_cast<unsigned>(category)) +
        " drop_sim_us=" +
            std::to_string(dropSimUs) +
        " dcc_age_us=" +
            std::to_string(dccAgeUs) +
        " reason=lifetime_expired");

    EraseContext(packet);
}

void
Ns3DccAdapter::SynchronizeRuntimeScheduler()
{
    ScheduleNextRuntimeEvent();
}

void
Ns3DccAdapter::ScheduleNextRuntimeEvent()
{
    if (m_runtimeEventScheduled)
    {
        m_runtimeEvent.Cancel();
        m_runtimeEventScheduled = false;
    }

    const auto now = m_runtime.now();
    const auto next = m_runtime.next();

    const auto nowUs =
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch())
            .count();

    const auto nextUs =
        std::chrono::duration_cast<std::chrono::microseconds>(
            next.time_since_epoch())
            .count();

    std::cout
        << "[DCC_RUNTIME]"
        << " now_us=" << nowUs
        << " next_us=" << nextUs;

    if (next != vanetza::Clock::time_point::max())
    {
        std::cout
            << " delta_us="
            << (nextUs - nowUs);
    }
    else
    {
        std::cout
            << " delta_us=NONE";
    }

    std::cout << std::endl;

    if (next == vanetza::Clock::time_point::max())
    {
        m_runtimeEventDeadline =
            vanetza::Clock::time_point::max();

        return;
    }

    if (next < now)
    {
        /*
         * O callback já está expirado. Não tentamos agendar no passado:
         * processamos o callback no próximo instante do Simulator.
         */
        m_runtimeEvent =
            Simulator::Schedule(
                Seconds(0),
                [this, next]()
                {
                    m_runtimeEventScheduled = false;

                    const auto current =
                        m_runtime.now();

                    const auto triggerTime =
                        (next < current) ? current : next;

                    std::cout
                        << "[DCC_RUNTIME_TRIGGER]"
                        << " sim_us="
                        << Simulator::Now()
                               .GetMicroSeconds()
                        << " runtime_us="
                        << std::chrono::duration_cast<
                               std::chrono::microseconds>(
                               current.time_since_epoch())
                               .count()
                        << " trigger_us="
                        << std::chrono::duration_cast<
                               std::chrono::microseconds>(
                               triggerTime.time_since_epoch())
                               .count()
                        << std::endl;

                    m_runtime.trigger(triggerTime);

                    ScheduleNextRuntimeEvent();
                });

        m_runtimeEventScheduled = true;
        m_runtimeEventDeadline = next;

        return;
    }

    const auto delta = next - now;

    const auto delayUs =
        std::chrono::duration_cast<
            std::chrono::microseconds>(delta)
            .count();

    const auto delay =
        MicroSeconds(
            static_cast<int64_t>(delayUs));

    m_runtimeEvent =
        Simulator::Schedule(
            delay,
            [this, next]()
            {
                m_runtimeEventScheduled = false;

                const auto current =
                    m_runtime.now();

                const auto triggerTime =
                    (next < current) ? current : next;

                std::cout
                    << "[DCC_RUNTIME_TRIGGER]"
                    << " sim_us="
                    << Simulator::Now()
                           .GetMicroSeconds()
                    << " runtime_us="
                    << std::chrono::duration_cast<
                           std::chrono::microseconds>(
                           current.time_since_epoch())
                           .count()
                    << " trigger_us="
                    << std::chrono::duration_cast<
                           std::chrono::microseconds>(
                           triggerTime.time_since_epoch())
                           .count()
                    << std::endl;

                m_runtime.trigger(triggerTime);

                ScheduleNextRuntimeEvent();
            });

    m_runtimeEventScheduled = true;
    m_runtimeEventDeadline = next;
}

void
Ns3DccAdapter::request(
    const vanetza::dcc::DataRequest& dccRequest,
    std::unique_ptr<vanetza::ChunkPacket> packet)
{
    if (!packet)
    {
        return;
    }

    PacketContextPtr ctx =
        TxBridgeContext::Current();

    const auto* packetKey =
        packet.get();

    if (ctx)
    {
        m_packetContexts[packetKey] = ctx;

        V2XWriteLog(
            *ctx,
            V2XLogLevel::DEBUG,
            "DCC",
            "DCC_REQUEST",
            "profile=" +
                std::to_string(
                    static_cast<unsigned>(
                        dccRequest.dcc_profile)));
    }

    /*
     * Delegate the DCC decision entirely to Vanetza::dcc::FlowControl.
     *
     * FlowControl decides whether the packet is:
     *
     *   1. transmitted immediately, or
     *   2. queued and associated with a future Runtime callback.
     *
     * No Profile -> AccessCategory mapping is performed here.
     */
    m_flowControl.request(
        dccRequest,
        std::move(packet));

    /*
     * FlowControl may have scheduled a callback in ManualRuntime.
     * Synchronize that callback with the ns-3 Simulator.
     */
    ScheduleNextRuntimeEvent();

    /*
     * The PacketContext is now owned by m_packetContexts for any queued
     * transmission. The bridge must not remain populated after returning
     * from FlowControl::request(), otherwise an unrelated/internal
     * transmission can inherit a stale application context.
     *
     * For an immediate transmission, Ns3AccessAdapter::request() has
     * already consumed the bridge, so Current() is null here.
     */
    if (ctx &&
        TxBridgeContext::Current() == ctx)
    {
        TxBridgeContext::Pop();
    }

    /*
     * For immediate transmission, the packet_transmit_hook has already
     * transferred the PacketContext to Ns3AccessAdapter through
     * TxBridgeContext. In that case there is no pending Runtime event
     * associated with this packet, so the local map entry can be removed.
     *
     * For queued packets, the context remains in m_packetContexts until
     * FlowControl invokes either the transmit or drop hook.
     */
    if (m_runtime.next() ==
        vanetza::Clock::time_point::max())
    {
        EraseContext(packetKey);
    }
}

void
Ns3DccAdapter::UpdateChannelLoad()
{
    if (!m_channelLoadProbe)
    {
        return;
    }

    const auto channelLoad =
        m_channelLoadProbe->Measure();

    const auto oldState =
        m_stateMachine.state();

    m_stateMachine.update(channelLoad);

    const auto newState =
        m_stateMachine.state();

    std::cout
        << "[DCC_CBR]"
        << " veh=" << m_vehicleId
        << " node=" << m_nodeId
        << " station=" << m_stationId
        << " sim_us="
        << Simulator::Now().GetMicroSeconds()
        << " cbr="
        << channelLoad.value()
        << " state_before="
        << oldState
        << " state_after="
        << newState
        << " rx_start="
        << m_channelLoadProbe->RxStartCount()
        << " tx_start="
        << m_channelLoadProbe->TxStartCount()
        << " cca_busy="
        << m_channelLoadProbe->CcaBusyStartCount()
        << std::endl;

    /*
     * Diagnostic observation of the exact Runtime deadline selected by
     * FlowControl before and after reschedule(). We do not alter Vanetza's
     * FlowControl implementation; we only observe the Runtime state.
     */
    const auto runtimeNow =
        m_runtime.now();

    const auto runtimeNextBefore =
        m_runtime.next();

    const auto toUs =
        [](const vanetza::Clock::time_point& tp) -> int64_t
        {
            if (tp == vanetza::Clock::time_point::max())
            {
                return -1;
            }

            return std::chrono::duration_cast<
                std::chrono::microseconds>(
                    tp.time_since_epoch())
                .count();
        };

    const auto delayUs =
        [&](const vanetza::Clock::time_point& tp) -> int64_t
        {
            if (tp == vanetza::Clock::time_point::max())
            {
                return -1;
            }

            return std::chrono::duration_cast<
                std::chrono::microseconds>(
                    tp - runtimeNow)
                .count();
        };

    const auto nextBeforeUs =
        toUs(runtimeNextBefore);

    const auto delayBeforeUs =
        delayUs(runtimeNextBefore);

    m_flowControl.reschedule();

    const auto runtimeNextAfter =
        m_runtime.next();

    const auto nextAfterUs =
        toUs(runtimeNextAfter);

    const auto delayAfterUs =
        delayUs(runtimeNextAfter);

    if (oldState != newState)
    {
        std::cout
            << "[DCC_RESCHEDULE]"
            << " veh=" << m_vehicleId
            << " node=" << m_nodeId
            << " station=" << m_stationId
            << " sim_us="
            << Simulator::Now().GetMicroSeconds()
            << " state_before="
            << oldState
            << " state_after="
            << newState
            << " runtime_now_us="
            << toUs(runtimeNow)
            << " next_before_us="
            << nextBeforeUs
            << " delay_before_us="
            << delayBeforeUs
            << " next_after_us="
            << nextAfterUs
            << " delay_after_us="
            << delayAfterUs
            << " changed="
            << ((nextBeforeUs != nextAfterUs ||
                 delayBeforeUs != delayAfterUs)
                    ? "yes"
                    : "no")
            << std::endl;
    }

    ScheduleNextChannelLoadMeasurement();
}

void
Ns3DccAdapter::ScheduleNextChannelLoadMeasurement()
{
    if (m_channelLoadEventScheduled)
    {
        m_channelLoadEvent.Cancel();
        m_channelLoadEventScheduled = false;
    }

    m_channelLoadEvent =
        Simulator::Schedule(
            MilliSeconds(100),
            [this]()
            {
                m_channelLoadEventScheduled = false;

                UpdateChannelLoad();
            });

    m_channelLoadEventScheduled = true;
}

} // namespace ns3
