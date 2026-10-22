#include "vanetza_integration/cpm/perception/cpm_temporal_transform.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace
{
bool Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << label << '\n';
    return condition;
}
}

int main()
{
    bool allPassed = true;
    std::string error;
    std::int64_t actual = -1;

    // 2026-10-22T10:00:00Z corresponde a
    // 719748000000000 us desde 2004-01-01T00:00:00Z.
    constexpr std::int64_t kKnownItsStartUs = 719748000000000LL;

    bool ok = CpmTemporalTransform::BuildReferenceTime(
        kKnownItsStartUs, 0, actual, error);

    allPassed &= Check(
        ok && actual == 719748000000LL && error.empty(),
        "known_utc_epoch_anchor");

    error.clear();
    actual = -1;
    ok = CpmTemporalTransform::BuildReferenceTime(
        kKnownItsStartUs, 100000, actual, error);

    allPassed &= Check(
        ok && actual == 719748000100LL && error.empty(),
        "known_sumo_step_100ms");

    error.clear();
    actual = -1;
    ok = CpmTemporalTransform::BuildReferenceTime(
        kKnownItsStartUs, 15000000, actual, error);

    allPassed &= Check(
        ok && actual == 719748015000LL && error.empty(),
        "known_sumo_elapsed_15s");

    error.clear();
    actual = -1;
    ok = CpmTemporalTransform::BuildReferenceTime(
        kKnownItsStartUs, 999, actual, error);

    allPassed &= Check(
        ok && actual == 719748000000LL && error.empty(),
        "submillisecond_truncation");

    error.clear();
    actual = -1;
    ok = CpmTemporalTransform::BuildReferenceTime(
        kKnownItsStartUs, 1000, actual, error);

    allPassed &= Check(
        ok && actual == 719748000001LL && error.empty(),
        "one_millisecond_boundary");

    std::cout << (allPassed
        ? "CPM_TEMPORAL_EPOCH_REFERENCE=PASS\n"
        : "CPM_TEMPORAL_EPOCH_REFERENCE=FAIL\n");

    return allPassed ? 0 : 1;
}
