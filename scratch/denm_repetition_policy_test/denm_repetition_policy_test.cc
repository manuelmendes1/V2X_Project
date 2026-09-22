#include "vanetza_integration/denm/scheduler/denm_repetition_policy.h"

#include <iostream>

using namespace ns3;

namespace
{

bool
Expect(bool condition, const char* description)
{
    if (condition)
    {
        std::cout << "PASS " << description << "\n";
        return true;
    }

    std::cout << "FAIL " << description << "\n";
    return false;
}

} // namespace

int main()
{
    bool ok = true;

    std::cout
        << "===== DENM REPETITION POLICY TEST =====\n";

    DenmRepetitionPolicy policy;

    policy.Start(
        1000000,
        200000,
        3);

    ok &= Expect(
        !policy.ShouldRepeat(1000000),
        "no repetition at initial transmission time");

    ok &= Expect(
        !policy.ShouldRepeat(1199999),
        "no repetition before interval");

    ok &= Expect(
        policy.ShouldRepeat(1200000),
        "first repetition at interval");

    policy.Consume(1200000);

    ok &= Expect(
        policy.RepetitionCount() == 1,
        "first repetition increments count");

    ok &= Expect(
        policy.NextRepetitionTimeUs() == 1400000,
        "next repetition keeps deterministic interval");

    ok &= Expect(
        !policy.ShouldRepeat(1399999),
        "second repetition not early");

    ok &= Expect(
        policy.ShouldRepeat(1400000),
        "second repetition at interval");

    policy.Consume(1400000);

    ok &= Expect(
        policy.RepetitionCount() == 2,
        "second repetition increments count");

    ok &= Expect(
        policy.ShouldRepeat(1600000),
        "third repetition becomes due");

    policy.Consume(1600000);

    ok &= Expect(
        policy.RepetitionCount() == 3,
        "maximum repetition count reached");

    ok &= Expect(
        !policy.ShouldRepeat(1800000),
        "no repetition after maximum count");

    DenmRepetitionPolicy invalidInterval;

    invalidInterval.Start(
        1000000,
        0,
        3);

    ok &= Expect(
        !invalidInterval.ShouldRepeat(1200000),
        "zero interval disables repetition");

    DenmRepetitionPolicy invalidCount;

    invalidCount.Start(
        1000000,
        200000,
        0);

    ok &= Expect(
        !invalidCount.ShouldRepeat(1200000),
        "zero maximum repetitions disables repetition");

    policy.Reset();

    ok &= Expect(
        !policy.ShouldRepeat(5000000),
        "reset disables repetition");

    if (!ok)
    {
        std::cout
            << "DENM_REPETITION_POLICY_EXIT=1\n";

        return 1;
    }

    std::cout
        << "DENM_REPETITION_POLICY_EXIT=0\n";

    return 0;
}
