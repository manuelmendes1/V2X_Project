#!/usr/bin/env bash

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ACCEPTANCE="$ROOT/scratch/vanetza_integration/tests/test_cam_regression_acceptance.sh"
LOG="${1:-/tmp/p9_13_cam_16s.log}"
SIM_DURATION="${2:-16}"

if [[ ! -f "$LOG" ]]; then
    echo "[FAIL] CAM horizon test log not found: $LOG"
    exit 2
fi

echo "============================================================"
echo "P9.13 — CAM HORIZON ACCEPTANCE REGRESSION"
echo "============================================================"
echo "LOG=$LOG"
echo "SIM_DURATION=$SIM_DURATION"

echo
echo "===== POSITIVE CONTROL ====="

set +e
POSITIVE_OUTPUT="$(
    bash "$ACCEPTANCE" "$LOG" 0 "$SIM_DURATION" 2>&1
)"
POSITIVE_RC=$?
set -e

printf '%s\n' "$POSITIVE_OUTPUT"

if [[ "$POSITIVE_RC" -ne 0 ]]; then
    echo
    echo "[FAIL] valid finite-horizon CAM run was rejected"
    echo "P9_13_CAM_HORIZON_ACCEPTANCE_REGRESSION=FAIL"
    exit 1
fi

if ! grep -qF "PENDING_AT_HORIZON" <<<"$POSITIVE_OUTPUT"; then
    echo
    echo "[FAIL] positive control did not exercise PENDING_AT_HORIZON"
    echo "P9_13_CAM_HORIZON_ACCEPTANCE_REGRESSION=FAIL"
    exit 1
fi

echo
echo "[PASS] valid finite-horizon CAM run accepted"

echo
echo "===== NEGATIVE CONTROL ====="

GOOD_PKT="$(
    grep -m1 -E \
        '\[PKT=[0-9]+\].*\[EV=TX_COMPLETE\]' \
        "$LOG" |
    sed -n 's/.*\[PKT=\([^]]*\)\].*/\1/p'
)"

if [[ -z "$GOOD_PKT" ]]; then
    echo "[FAIL] could not identify a completed CAM packet"
    echo "P9_13_CAM_HORIZON_ACCEPTANCE_REGRESSION=FAIL"
    exit 1
fi

BAD_LOG="/tmp/p9_13_cam_horizon_bad.log"

sed -E \
    "/\[PKT=${GOOD_PKT}\].*\[EV=TX_COMPLETE\]/d" \
    "$LOG" \
    > "$BAD_LOG"

set +e
NEGATIVE_OUTPUT="$(
    bash "$ACCEPTANCE" "$BAD_LOG" 0 "$SIM_DURATION" 2>&1
)"
NEGATIVE_RC=$?
set -e

printf '%s\n' "$NEGATIVE_OUTPUT"

rm -f "$BAD_LOG"

if [[ "$NEGATIVE_RC" -eq 0 ]]; then
    echo
    echo "[FAIL] acceptance accepted a packet with DCC_TX but no TX_COMPLETE"
    echo "P9_13_CAM_HORIZON_ACCEPTANCE_REGRESSION=FAIL"
    exit 1
fi

echo
echo "[PASS] incomplete post-DCC TX is still rejected"

echo
echo "P9_13_CAM_HORIZON_ACCEPTANCE_REGRESSION=PASS"
exit 0
