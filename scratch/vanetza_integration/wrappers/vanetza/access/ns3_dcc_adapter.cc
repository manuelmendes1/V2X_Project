#include "ns3_dcc_adapter.h"
#include "vanetza_integration/context/tx_bridge/tx_binding_outcome.h"

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
    vanetza::Clock::time_point itsStartTime,
    std::string endpointId,
    uint32_t nodeId,
    uint32_t stationId,
    std::string identityLabel)
    : m_access(access),
      m_runtime(runtime),
      m_itsStartTime(itsStartTime),
      m_endpointId(std::move(endpointId)),
      m_identityLabel(std::move(identityLabel)),
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

    // Resolve outstanding completion transactions before the DCC queue
    // and its dependent adapters are destroyed.
    auto pendingBindings = m_packetBindings.Drain();
    const auto shutdownSimUs =
        Simulator::Now().GetMicroSeconds();

    for (auto& binding : pendingBindings)
    {
        if (!binding.context && !binding.completion)
        {
            continue;
        }

        ResolveTxBindingDrop(
            binding,
            shutdownSimUs,
            "dcc_adapter_shutdown",
            "DCC adapter shutdown before transmission");

        if (binding.context)
        {
            V2XWriteLog(
                *binding.context,
                V2XLogLevel::WARN,
                "DCC",
                "DCC_DROP",
                "reason=dcc_adapter_shutdown");
        }
    }
}

void
Ns3DccAdapter::AdvanceRuntimeToSimulatorNow()
{
    const auto simulationOffset =
        std::chrono::microseconds(
            static_cast<vanetza::Clock::rep>(
                Simulator::Now().GetMicroSeconds()));

    const auto targetTime =
        m_itsStartTime + simulationOffset;

    // ManualRuntime::trigger() also executes callbacks whose deadlines
    // have expired. Never move its clock backwards.
    if (targetTime > m_runtime.now())
    {
        m_runtime.trigger(targetTime);
    }
}

void
Ns3DccAdapter::OnPacketTransmit(
    vanetza::access::AccessCategory category,
    const vanetza::ChunkPacket* packet)
{
    TxBridgeBinding binding =
        m_packetBindings.Take(packet);

    if (!binding.context && !binding.completion)
    {
        return;
    }

    /*
     * FlowControl invokes this hook immediately before forwarding the
     * packet to the access layer. Publish the complete binding so the
     * Access adapter can retain it through the asynchronous send.
     */
    TxBridgeContext::Push(
        binding.context,
        binding.completion);

    if (!binding.context)
    {
        return;
    }

    const auto txSimUs =
        Simulator::Now().GetMicroSeconds();

    const auto requestSimUs =
        static_cast<int64_t>(
            binding.context->simulationTime * 1'000'000.0);

    const auto dccDelayUs =
        txSimUs - requestSimUs;

    V2XWriteLog(
        *binding.context,
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
}

void
Ns3DccAdapter::OnPacketDrop(
    vanetza::access::AccessCategory category,
    const vanetza::ChunkPacket* packet)
{
    TxBridgeBinding binding =
        m_packetBindings.Take(packet);

    if (!binding.context && !binding.completion)
    {
        return;
    }

    const auto dropSimUs =
        Simulator::Now().GetMicroSeconds();

    ResolveTxBindingDrop(
        binding,
        dropSimUs,
        "dcc_lifetime_expired",
        "DCC packet lifetime expired");

    if (binding.context)
    {
        const auto requestSimUs =
            static_cast<int64_t>(
                binding.context->simulationTime * 1'000'000.0);

        const auto dccAgeUs =
            dropSimUs - requestSimUs;

        V2XWriteLog(
            *binding.context,
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
    }
}

void
Ns3DccAdapter::SynchronizeRuntimeScheduler()
{
    ScheduleNextRuntimeEvent();
}

void
Ns3DccAdapter::SynchronizeRuntimeForTransmit()
{
    // Advance the absolute ITS clock before Router::request() builds
    // the GeoNetworking source position vector.
    AdvanceRuntimeToSimulatorNow();

    // Advancing ManualRuntime may execute callbacks and change its
    // earliest deadline. Keep the ns-3 scheduler aligned afterwards.
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
    // Consume the complete application binding at the DCC boundary.
    TxBridgeBinding binding = TxBridgeContext::Take();

    const auto failBinding = [](
        TxBridgeBinding& failedBinding,
        int64_t completedAtSimUs,
        const std::string& reason)
    {
        ResolveTxBindingFailure(
            failedBinding,
            completedAtSimUs,
            reason);

        if (failedBinding.context)
        {
            V2XWriteLog(
                *failedBinding.context,
                V2XLogLevel::ERROR,
                "DCC",
                "DCC_REQUEST_FAIL",
                "reason=" + reason);
        }
    };

    if (!packet)
    {
        failBinding(
            binding,
            Simulator::Now().GetMicroSeconds(),
            "dcc_null_chunk_packet");
        return;
    }

    const auto* packetKey = packet.get();

    if (binding.context || binding.completion)
    {
        // Keep the local copy in case the registry rejects a duplicate key.
        if (!m_packetBindings.Store(packetKey, binding))
        {
            failBinding(
                binding,
                Simulator::Now().GetMicroSeconds(),
                "dcc_packet_binding_duplicate");
            return;
        }

        if (binding.context)
        {
            V2XWriteLog(
                *binding.context,
                V2XLogLevel::DEBUG,
                "DCC",
                "DCC_REQUEST",
                "profile=" +
                    std::to_string(
                        static_cast<unsigned>(
                            dccRequest.dcc_profile)));
        }
    }

    /*
     * FlowControl decides whether to transmit immediately or queue.
     * The registry retains the context/completion pair while queued.
     */
    AdvanceRuntimeToSimulatorNow();

    m_flowControl.request(
        dccRequest,
        std::move(packet));

    ScheduleNextRuntimeEvent();

}

void
Ns3DccAdapter::UpdateChannelLoad()
{
    if (!m_channelLoadProbe)
    {
        return;
    }

    AdvanceRuntimeToSimulatorNow();

    const auto channelLoad =
        m_channelLoadProbe->Measure();

    const auto oldState =
        m_stateMachine.state();

    m_stateMachine.update(channelLoad);

    const auto newState =
        m_stateMachine.state();

    std::cout
        << "[DCC_CBR]"
        << " " << m_identityLabel << "=" << m_endpointId
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

    // FlowControl may have replaced its pending callback. Keep the ns-3
    // EventId aligned with the new earliest ManualRuntime deadline.
    ScheduleNextRuntimeEvent();

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
            << " " << m_identityLabel << "=" << m_endpointId
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
