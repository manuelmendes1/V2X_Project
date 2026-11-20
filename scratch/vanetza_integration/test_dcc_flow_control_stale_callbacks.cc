#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

#include <vanetza/access/data_request.hpp>
#include <vanetza/access/interface.hpp>
#include <vanetza/common/clock.hpp>
#include <vanetza/common/manual_runtime.hpp>
#include <vanetza/dcc/data_request.hpp>
#include <vanetza/dcc/flow_control.hpp>
#include <vanetza/dcc/gradual_state_machine.hpp>
#include <vanetza/dcc/single_reactive_transmit_rate_control.hpp>
#include <vanetza/net/chunk_packet.hpp>

namespace
{
using namespace std::chrono_literals;
using vanetza::Clock;
using vanetza::ChunkPacket;
using vanetza::ManualRuntime;

struct RecordingAccess final : vanetza::access::Interface
{
    explicit RecordingAccess(ManualRuntime& runtime) : runtime(runtime) {}

    void request(
        const vanetza::access::DataRequest&,
        std::unique_ptr<ChunkPacket>) override
    {
        sentAt.push_back(runtime.now());
    }

    ManualRuntime& runtime;
    std::vector<Clock::time_point> sentAt;
};

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    return condition;
}

vanetza::dcc::DataRequest MakeRequest(Clock::duration lifetime)
{
    vanetza::dcc::DataRequest request;
    request.dcc_profile = vanetza::dcc::Profile::DP0;
    request.lifetime = lifetime;
    return request;
}

} // namespace

int main()
{
    ManualRuntime runtime(Clock::time_point{});

    vanetza::dcc::GradualStateMachine fsm(vanetza::dcc::etsiStates1ms);
    vanetza::dcc::SingleReactiveTransmitRateControl trc(fsm, runtime);

    RecordingAccess access(runtime);
    vanetza::dcc::FlowControl flow(runtime, trc, access);

    std::size_t dropped = 0;
    std::size_t transmitHooks = 0;

    flow.set_packet_drop_hook(
        [&](vanetza::access::AccessCategory, const ChunkPacket*)
        {
            ++dropped;
        });

    flow.set_packet_transmit_hook(
        [&](vanetza::access::AccessCategory, const ChunkPacket*)
        {
            ++transmitHooks;
        });

    auto submit = [&](Clock::duration lifetime)
    {
        flow.request(
            MakeRequest(lifetime),
            std::make_unique<ChunkPacket>());
    };

    std::cout << "===== DCC STALE CALLBACK REGRESSION =====\n";

    // Establish last_tx = 0. The first request must transmit immediately.
    submit(1s);

    const bool initialTx = access.sentAt.size() == 1;

    // A is queued for transmission at t=100 ms, but expires at t=50 ms.
    submit(50ms);

    // At t=75 ms, A expires. B becomes the first queued packet and
    // schedules a second callback for t=100 ms.
    runtime.trigger(75ms);
    submit(1s);

    // C joins B's queue. It does not schedule another callback.
    submit(1s);

    const bool aWasDropped = dropped == 1;

    // At t=100 ms, both callbacks are due.
    runtime.trigger(25ms);

    const std::size_t transmissionsAt100ms = access.sentAt.size();

    // The next legitimate queued transmission should occur at t=200 ms.
    runtime.trigger(100ms);

    const std::size_t transmissionsAt200ms = access.sentAt.size();

    std::cout << "OBS transmissions_at_100ms="
              << transmissionsAt100ms << '\n';
    std::cout << "OBS transmissions_at_200ms="
              << transmissionsAt200ms << '\n';
    std::cout << "OBS transmit_hooks=" << transmitHooks << '\n';
    std::cout << "OBS dropped=" << dropped << '\n';
    std::cout << "OBS sent_at_ms=";

    for (std::size_t i = 0; i < access.sentAt.size(); ++i)
    {
        if (i != 0)
        {
            std::cout << ',';
        }

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                access.sentAt[i] - Clock::time_point{}).count();

        std::cout << elapsed;
    }

    std::cout << '\n';

    const bool oneQueuedTxAt100ms = transmissionsAt100ms == 2;

    const bool cTransmittedAt200ms =
        transmissionsAt200ms == 3 &&
        access.sentAt.size() == 3 &&
        access.sentAt[2] == Clock::time_point{} + 200ms;

    bool passed = true;
    passed &= Check(initialTx, "INITIAL_PACKET_TRANSMITTED_IMMEDIATELY");
    passed &= Check(aWasDropped, "EXPIRED_PACKET_A_DROPPED");
    passed &= Check(
        oneQueuedTxAt100ms,
        "ONLY_ONE_QUEUED_PACKET_TRANSMITTED_AT_100MS");
    passed &= Check(
        cTransmittedAt200ms,
        "NEXT_QUEUED_PACKET_TRANSMITTED_AT_200MS");

    std::cout << (passed
        ? "DCC_FLOWCONTROL_STALE_CALLBACK=PASS\n"
        : "DCC_FLOWCONTROL_STALE_CALLBACK=FAIL\n");

    return passed ? 0 : 1;
}
