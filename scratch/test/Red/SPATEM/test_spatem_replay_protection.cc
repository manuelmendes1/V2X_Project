#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/spatem/replay/spatem_replay_protection.h"
#include "vanetza_integration/spatem/types/spatem_message.h"

void V2XWriteLog(
    const ns3::PacketContext&,
    V2XLogLevel,
    const std::string&,
    const std::string&,
    const std::string&)
{
}

namespace
{

SpatemMessage MakeMessage(
    std::optional<uint32_t> minuteOfYear,
    std::optional<uint16_t> dSecond,
    uint16_t intersectionId = 42)
{
    SpatemMessage message;
    message.stationId = 9001;

    SpatemIntersection intersection;
    intersection.intersectionId = intersectionId;
    intersection.minuteOfYear = minuteOfYear;
    intersection.dSecond = dSecond;

    message.intersections.push_back(intersection);
    return message;
}

ns3::PacketContext MakeContext(
    uint64_t receiveTimestamp,
    std::vector<uint8_t> payload)
{
    ns3::PacketContext context;
    context.timestamp = receiveTimestamp;
    context.payload.bytes = std::move(payload);
    return context;
}

bool Check(
    const std::string& name,
    bool passed,
    const std::string& details)
{
    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << name << " " << details << '\n';
    return passed;
}

bool TestIdenticalSemanticDuplicateIsRejected()
{
    ns3::SpatemReplayContext replay;
    const auto message = MakeMessage(20, 1000);

    auto first = MakeContext(1000000, {0x10, 0x20, 0x30});
    const bool firstAccepted =
        ns3::SpatemReplayProtection::Process(first, message, replay);

    auto duplicate = MakeContext(1000001, {0x10, 0x20, 0x30});
    const bool duplicateAccepted =
        ns3::SpatemReplayProtection::Process(
            duplicate, message, replay);

    const bool passed =
        firstAccepted &&
        !duplicateAccepted &&
        duplicate.reason == "spatem_replay_detected" &&
        replay.database.size() == 1 &&
        replay.database.begin()->second.fingerprints.size() == 1;

    return Check(
        "identical_semantic_duplicate_rejected",
        passed,
        "first_accepted=" + std::to_string(firstAccepted) +
        " duplicate_accepted=" + std::to_string(duplicateAccepted) +
        " reason=" + duplicate.reason +
        " entries=" + std::to_string(replay.database.size()));
}

bool TestOlderSemanticTimestampIsRejectedWithoutMutation()
{
    ns3::SpatemReplayContext replay;

    const auto newest = MakeMessage(20, 2000);
    auto first = MakeContext(1000000, {0x20, 0x01});

    const bool firstAccepted =
        ns3::SpatemReplayProtection::Process(first, newest, replay);

    const auto older = MakeMessage(20, 1000);
    auto second = MakeContext(1000001, {0x20, 0x02});

    const bool olderAccepted =
        ns3::SpatemReplayProtection::Process(second, older, replay);

    const bool passed =
        firstAccepted &&
        !olderAccepted &&
        second.reason == "spatem_timestamp_older_than_last" &&
        replay.database.size() == 1 &&
        replay.database.begin()->second.timestamp == 1202000ULL &&
        replay.database.begin()->second.fingerprints.size() == 1;

    return Check(
        "older_semantic_timestamp_rejected_without_mutation",
        passed,
        "older_accepted=" + std::to_string(olderAccepted) +
        " reason=" + second.reason +
        " entries=" + std::to_string(replay.database.size()));
}

bool TestDifferentTimestampDomainsKeepIndependentHistory()
{
    ns3::SpatemReplayContext replay;

    // Semantic time: 20 minutes plus 1000 milliseconds.
    const auto semantic = MakeMessage(20, 1000);
    auto first = MakeContext(1000000, {0x30, 0x01});

    const bool firstAccepted =
        ns3::SpatemReplayProtection::Process(first, semantic, replay);

    // No semantic time: context.timestamp is ns-3 simulation time in us.
    const auto noSemanticTime = MakeMessage(std::nullopt, std::nullopt);
    auto second = MakeContext(1000001, {0x30, 0x02});

    const bool secondAccepted =
        ns3::SpatemReplayProtection::Process(
            second, noSemanticTime, replay);

    // The domain change must not erase the first semantic replay record.
    auto repeatedSemantic = MakeContext(1000002, {0x30, 0x01});
    const bool repeatedSemanticAccepted =
        ns3::SpatemReplayProtection::Process(
            repeatedSemantic, semantic, replay);

    const bool passed =
        firstAccepted &&
        secondAccepted &&
        !repeatedSemanticAccepted &&
        repeatedSemantic.reason == "spatem_replay_detected" &&
        replay.database.size() == 2;

    return Check(
        "different_timestamp_domains_keep_independent_history",
        passed,
        "semantic_accepted=" + std::to_string(firstAccepted) +
        " no_semantic_accepted=" + std::to_string(secondAccepted) +
        " repeated_semantic_accepted=" +
            std::to_string(repeatedSemanticAccepted) +
        " repeated_reason=" + repeatedSemantic.reason +
        " entries=" + std::to_string(replay.database.size()));
}

bool TestIdenticalNonSemanticDuplicateIsRejectedAcrossReceiveTimestamps()
{
    ns3::SpatemReplayContext replay;
    const auto message = MakeMessage(std::nullopt, std::nullopt);

    auto first = MakeContext(1000000, {0x50, 0x01, 0x02});
    const bool firstAccepted =
        ns3::SpatemReplayProtection::Process(first, message, replay);

    // Same payload, received 100 us later, without semantic time.
    auto duplicate = MakeContext(1000100, {0x50, 0x01, 0x02});
    const bool duplicateAccepted =
        ns3::SpatemReplayProtection::Process(
            duplicate, message, replay);

    const bool passed =
        firstAccepted &&
        !duplicateAccepted &&
        duplicate.reason == "spatem_replay_detected" &&
        replay.database.size() == 1 &&
        replay.database.begin()->second.fingerprints.size() == 1;

    return Check(
        "identical_nonsemantic_duplicate_rejected",
        passed,
        "first_accepted=" + std::to_string(firstAccepted) +
        " duplicate_accepted=" + std::to_string(duplicateAccepted) +
        " reason=" + duplicate.reason +
        " entries=" + std::to_string(replay.database.size()));
}

bool TestIdenticalNonSemanticPayloadAfterWindowIsAccepted()
{
    ns3::SpatemReplayContext replay;
    const auto message = MakeMessage(std::nullopt, std::nullopt);

    auto first = MakeContext(1000000, {0x51, 0x01, 0x02});
    const bool firstAccepted =
        ns3::SpatemReplayProtection::Process(first, message, replay);

    // 1001 us is outside the configured 1000 us duplicate window.
    auto later = MakeContext(1001001, {0x51, 0x01, 0x02});
    const bool laterAccepted =
        ns3::SpatemReplayProtection::Process(later, message, replay);

    const bool passed =
        firstAccepted &&
        laterAccepted &&
        later.reason != "spatem_replay_detected" &&
        replay.database.size() == 1 &&
        replay.database.begin()->second.timestamp == 1001001ULL &&
        replay.database.begin()->second.fingerprints.size() == 1;

    return Check(
        "identical_nonsemantic_payload_after_window_accepted",
        passed,
        "first_accepted=" + std::to_string(firstAccepted) +
        " later_accepted=" + std::to_string(laterAccepted) +
        " reason=" + later.reason +
        " entries=" + std::to_string(replay.database.size()));
}

bool TestMultiIntersectionRejectionIsAtomic()
{
    ns3::SpatemReplayContext replay;

    const auto baseline = MakeMessage(20, 2000, 43);
    auto first = MakeContext(1000000, {0x40, 0x01});

    const bool baselineAccepted =
        ns3::SpatemReplayProtection::Process(first, baseline, replay);

    SpatemMessage mixed;
    mixed.stationId = 9001;

    SpatemIntersection newIntersection;
    newIntersection.intersectionId = 42;
    newIntersection.minuteOfYear = 20;
    newIntersection.dSecond = 3000;

    SpatemIntersection replayedIntersection;
    replayedIntersection.intersectionId = 43;
    replayedIntersection.minuteOfYear = 20;
    replayedIntersection.dSecond = 1000;

    mixed.intersections.push_back(newIntersection);
    mixed.intersections.push_back(replayedIntersection);

    auto candidate = MakeContext(1000001, {0x40, 0x02});
    const bool mixedAccepted =
        ns3::SpatemReplayProtection::Process(
            candidate, mixed, replay);

    bool intersection42Inserted = false;
    for (const auto& item : replay.database)
    {
        if (item.first.intersectionId == 42)
        {
            intersection42Inserted = true;
        }
    }

    const bool passed =
        baselineAccepted &&
        !mixedAccepted &&
        candidate.reason == "spatem_timestamp_older_than_last" &&
        replay.database.size() == 1 &&
        !intersection42Inserted;

    return Check(
        "multi_intersection_rejection_is_atomic",
        passed,
        "mixed_accepted=" + std::to_string(mixedAccepted) +
        " reason=" + candidate.reason +
        " entries=" + std::to_string(replay.database.size()) +
        " new_intersection_inserted=" +
            std::to_string(intersection42Inserted));
}

} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    for (const bool result : {
             TestIdenticalSemanticDuplicateIsRejected(),
             TestOlderSemanticTimestampIsRejectedWithoutMutation(),
             TestDifferentTimestampDomainsKeepIndependentHistory(),
             TestIdenticalNonSemanticDuplicateIsRejectedAcrossReceiveTimestamps(),
             TestIdenticalNonSemanticPayloadAfterWindowIsAccepted(),
             TestMultiIntersectionRejectionIsAtomic()})
    {
        if (result)
        {
            ++passed;
        }
        else
        {
            ++failed;
        }
    }

    std::cout << "SPATEM_REPLAY_CASES=" << passed + failed << '\n';
    std::cout << "SPATEM_REPLAY_PASSED=" << passed << '\n';
    std::cout << "SPATEM_REPLAY_FAILED=" << failed << '\n';
    std::cout << "SPATEM_REPLAY_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
