#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

TESTS=(
    "$ROOT/scratch/denm_bridge_detector_integration_test/denm_bridge_detector_integration_test.py"
    "$ROOT/scratch/denm_bridge_emission_test/denm_bridge_emission_test.py"
    "$ROOT/scratch/denm_bridge_main_integration_test/denm_bridge_main_integration_test.py"
    "$ROOT/scratch/denm_bridge_vehicle_processing_test/denm_bridge_vehicle_processing_test.py"
    "$ROOT/scratch/denm_hazard_detector_recovery_test/denm_hazard_detector_recovery_test.py"
)

failures=0

echo "===== DENM BRIDGE ACCEPTANCE ====="
echo "ROOT=$ROOT"

if ! command -v python3 >/dev/null 2>&1; then
    echo "FAIL: python3 is not available"
    echo "DENM_BRIDGE_ACCEPTANCE_EXIT=1"
    exit 1
fi

bridge="$ROOT/../v2x_sumo/integration/v2x_bridge_15.py"

if [ ! -f "$bridge" ]; then
    echo "FAIL: SUMO bridge not found: $bridge"
    echo "DENM_BRIDGE_ACCEPTANCE_EXIT=1"
    exit 1
fi

for test in "${TESTS[@]}"; do
    test_dir="$(dirname "$test")"
    test_file="$(basename "$test")"

    echo
    echo "===== TEST: $test_file ====="
    echo "CWD=$test_dir"

    if [ ! -f "$test" ]; then
        echo "FAIL: test not found: $test"
        failures=$((failures + 1))
        continue
    fi

    (
        cd "$test_dir" || exit 99
        python3 "./$test_file"
    )

    rc=$?

    echo "TEST_EXIT=$rc"

    if [ "$rc" -ne 0 ]; then
        failures=$((failures + 1))
    fi
done

echo
echo "===== RESULT ====="
echo "TEST_COUNT=${#TESTS[@]}"
echo "FAILURES=$failures"
echo "DENM_BRIDGE_ACCEPTANCE_EXIT=$((failures == 0 ? 0 : 1))"

exit $((failures == 0 ? 0 : 1))
