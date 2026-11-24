#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "vanetza_integration/cam/types/cam_input.h"
#include "vanetza_integration/context/vehicle_security/replay/replay_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/rx/rx_decode/rx_decode.h"
#include "vanetza_integration/security/replay/replay_protection.h"
#include "vanetza_integration/wrappers/vanetza/cam/vanetza_cam_codec.h"

namespace
{

unsigned g_passed = 0;
unsigned g_failed = 0;

void Expect(const std::string& marker, bool condition)
{
    if (condition)
    {
        ++g_passed;
        std::cout << "PASS " << marker << '\n';
    }
    else
    {
        ++g_failed;
        std::cout << "FAIL " << marker << '\n';
    }
}

CamInput MakeCamInput(uint32_t stationId, uint16_t generationDeltaTime)
{
    CamInput input{};
    input.stationId = stationId;
    input.latitude = 38.7223;
    input.longitude = -9.1393;
    input.speed = 12.5;
    input.heading = 90.0;
    input.acceleration = 0.0;
    input.generationDeltaTime = generationDeltaTime;
    return input;
}

bool SetCamPayload(
    ns3::PacketContext& ctx,
    uint32_t claimedStationId,
    uint16_t generationDeltaTime,
    uint32_t resolvedSenderStationId,
    uint64_t receptionTimestamp)
{
    const auto encoded = VanetzaCamCodec::Encode(
        MakeCamInput(claimedStationId, generationDeltaTime));

    if (!encoded.success || encoded.bytes.empty())
    {
        std::cerr << "CAM_TEST_FIXTURE_ENCODE=FAIL station="
                  << claimedStationId << "\n";
        return false;
    }

    ctx.senderStationId = resolvedSenderStationId;
    ctx.timestamp = receptionTimestamp;
    ctx.payload.type = ns3::PayloadType::CAM;
    ctx.payload.bytes = encoded.bytes;
    ctx.payload.signatureSize = 0;
    return true;
}

struct DecodeReplayOutcome
{
    bool decoded = false;
    bool replayInvoked = false;
    bool accepted = false;
};

// Executa a fronteira de produção DECODE -> REPLAY.
// O teste não simula VERIFY, RxValidation, resolução de endpoints ou DELIVER.
DecodeReplayOutcome ProcessDecodeThenReplay(
    ns3::PacketContext& ctx,
    ns3::ReplayContext& replay,
    CamMessage& message)
{
    DecodeReplayOutcome outcome;

    ctx.AdvancePipeline(ns3::PipelineStage::DECODE);
    outcome.decoded = RxDecode::Process(ctx, ctx.payload.bytes, message);

    if (!outcome.decoded)
    {
        return outcome;
    }

    ctx.MarkSuccess();
    ctx.AdvancePipeline(ns3::PipelineStage::REPLAY);
    outcome.replayInvoked = true;
    outcome.accepted = ns3::ReplayProtection::Process(
        ctx, message, replay);

    if (outcome.accepted)
    {
        ctx.MarkSuccess();
    }

    return outcome;
}

bool SameReplayEntry(
    const ns3::ReplayEntry& lhs,
    const ns3::ReplayEntry& rhs)
{
    return lhs.lastGenerationDeltaTime == rhs.lastGenerationDeltaTime &&
           lhs.lastReceptionTimestamp == rhs.lastReceptionTimestamp &&
           lhs.fingerprints == rhs.fingerprints;
}

void TestMismatchedStationIdCannotPoisonReplayState()
{
    constexpr uint32_t resolvedSenderStationId = 1001;
    constexpr uint32_t claimedCamStationId = 2002;

    ns3::PacketContext ctx;
    ns3::ReplayContext replay;

    ns3::ReplayEntry& victimEntry =
        replay.database[claimedCamStationId];
    victimEntry.lastGenerationDeltaTime = 100;
    victimEntry.lastReceptionTimestamp = 1'000'000;
    victimEntry.fingerprints.insert(0x1122334455667788ULL);
    const ns3::ReplayEntry victimBefore = victimEntry;

    if (!SetCamPayload(
            ctx,
            claimedCamStationId,
            200,
            resolvedSenderStationId,
            1'500'000))
    {
        Expect("CAM_MISMATCH_FIXTURE_ENCODED", false);
        Expect("CAM_MISMATCH_REJECTED_AT_DECODE", false);
        Expect("CAM_MISMATCH_BLOCKED_BEFORE_REPLAY", false);
        Expect("CAM_MISMATCH_PRESERVED_CONTEXT_IDENTITY", false);
        return;
    }

    Expect("CAM_MISMATCH_FIXTURE_ENCODED", true);

    CamMessage message{};
    const DecodeReplayOutcome outcome =
        ProcessDecodeThenReplay(ctx, replay, message);

    Expect(
        "CAM_MISMATCH_REJECTED_AT_DECODE",
        !outcome.decoded &&
        ctx.pipelineStage == ns3::PipelineStage::DECODE &&
        ctx.pipelineResult == ns3::PipelineResult::DROP &&
        ctx.reason == "cam_sender_identity_mismatch" &&
        message.stationId == claimedCamStationId);

    const auto victimAfter = replay.database.find(claimedCamStationId);
    const bool victimUnchanged =
        victimAfter != replay.database.end() &&
        SameReplayEntry(victimBefore, victimAfter->second) &&
        replay.database.size() == 1 &&
        replay.database.find(resolvedSenderStationId) == replay.database.end();

    Expect(
        "CAM_MISMATCH_BLOCKED_BEFORE_REPLAY",
        !outcome.replayInvoked && victimUnchanged);

    Expect(
        "CAM_MISMATCH_PRESERVED_CONTEXT_IDENTITY",
        ctx.senderStationId == resolvedSenderStationId);
}

void TestMissingContextSenderStationIdFailsClosed()
{
    constexpr uint32_t claimedCamStationId = 3003;

    ns3::PacketContext ctx;
    ns3::ReplayContext replay;

    if (!SetCamPayload(
            ctx,
            claimedCamStationId,
            300,
            0,
            2'000'000))
    {
        Expect("CAM_MISSING_SENDER_FIXTURE_ENCODED", false);
        Expect("CAM_MISSING_SENDER_ID_FAIL_CLOSED", false);
        return;
    }

    Expect("CAM_MISSING_SENDER_FIXTURE_ENCODED", true);

    CamMessage message{};
    const DecodeReplayOutcome outcome =
        ProcessDecodeThenReplay(ctx, replay, message);

    Expect(
        "CAM_MISSING_SENDER_ID_FAIL_CLOSED",
        !outcome.decoded &&
        !outcome.replayInvoked &&
        ctx.pipelineStage == ns3::PipelineStage::DECODE &&
        ctx.pipelineResult == ns3::PipelineResult::FAIL &&
        ctx.reason == "cam_sender_identity_missing" &&
        message.stationId == claimedCamStationId &&
        replay.database.empty());
}

void TestMatchingStationIdReachesReplay()
{
    constexpr uint32_t stationId = 4242;
    constexpr uint16_t generationDeltaTime = 4321;
    constexpr uint64_t receptionTimestamp = 2'500'000;

    ns3::PacketContext ctx;
    ns3::ReplayContext replay;

    if (!SetCamPayload(
            ctx,
            stationId,
            generationDeltaTime,
            stationId,
            receptionTimestamp))
    {
        Expect("CAM_MATCH_FIXTURE_ENCODED", false);
        Expect("CAM_MATCH_REACHES_REPLAY_AND_UPDATES_OWN_ENTRY", false);
        return;
    }

    Expect("CAM_MATCH_FIXTURE_ENCODED", true);

    CamMessage message{};
    const DecodeReplayOutcome outcome =
        ProcessDecodeThenReplay(ctx, replay, message);

    const auto acceptedEntry = replay.database.find(stationId);
    const bool replayStateUpdated =
        acceptedEntry != replay.database.end() &&
        acceptedEntry->second.lastGenerationDeltaTime == generationDeltaTime &&
        acceptedEntry->second.lastReceptionTimestamp == receptionTimestamp &&
        !acceptedEntry->second.fingerprints.empty() &&
        replay.database.size() == 1;

    Expect(
        "CAM_MATCH_REACHES_REPLAY_AND_UPDATES_OWN_ENTRY",
        outcome.decoded &&
        outcome.replayInvoked &&
        outcome.accepted &&
        ctx.pipelineStage == ns3::PipelineStage::REPLAY &&
        ctx.pipelineResult == ns3::PipelineResult::OK &&
        message.stationId == stationId &&
        ctx.senderStationId == stationId &&
        replayStateUpdated);
}

void TestZeroCamStationIdRemainsInvalid()
{
    ns3::PacketContext ctx;
    ns3::ReplayContext replay;

    if (!SetCamPayload(ctx, 0, 500, 5151, 3'000'000))
    {
        Expect("CAM_ZERO_STATION_FIXTURE_ENCODED", false);
        Expect("CAM_ZERO_STATION_ID_REMAINS_INVALID", false);
        return;
    }

    Expect("CAM_ZERO_STATION_FIXTURE_ENCODED", true);

    CamMessage message{};
    const DecodeReplayOutcome outcome =
        ProcessDecodeThenReplay(ctx, replay, message);

    Expect(
        "CAM_ZERO_STATION_ID_REMAINS_INVALID",
        !outcome.decoded &&
        !outcome.replayInvoked &&
        ctx.pipelineStage == ns3::PipelineStage::DECODE &&
        ctx.pipelineResult == ns3::PipelineResult::FAIL &&
        ctx.reason == "decode_failed" &&
        message.stationId == 0 &&
        replay.database.empty());
}

} // namespace

// Stubs locais para manter o teste isolado do sistema global de logging.
void V2XWriteLog(
    const ns3::PacketContext&,
    V2XLogLevel,
    const std::string&,
    const std::string&,
    const std::string&)
{
}

V2XTimePoint V2XTimeNow()
{
    return V2XClock::now();
}

int64_t V2XElapsedMicros(const V2XTimePoint& start)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        V2XClock::now() - start).count();
}

int main()
{
    TestMismatchedStationIdCannotPoisonReplayState();
    TestMissingContextSenderStationIdFailsClosed();
    TestMatchingStationIdReachesReplay();
    TestZeroCamStationIdRemainsInvalid();

    std::cout << "CAM_RX_IDENTITY_BINDING_PASSED=" << g_passed << '\n';
    std::cout << "CAM_RX_IDENTITY_BINDING_FAILED=" << g_failed << '\n';
    std::cout << "CAM_RX_IDENTITY_BINDING="
              << (g_failed == 0 ? "PASS" : "RED_EXPECTED") << '\n';

    // O RED funcional é recolhido pelos asserts unittest por cenário.
    // O exit code 0 indica apenas que o harness executou até ao fim.
    return 0;
}
