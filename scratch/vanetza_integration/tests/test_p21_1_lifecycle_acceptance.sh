#!/usr/bin/env bash
set -euo pipefail

LOG="${1:-test-results/p21/p21_1_lifecycle_ns3.log}"

if [[ ! -f "$LOG" ]]; then
    echo "FAIL P21.1 lifecycle acceptance: log not found: $LOG"
    exit 1
fi

echo "============================================================"
echo "P21.1 — LIFECYCLE ACCEPTANCE"
echo "============================================================"

echo
echo "===== LOG ====="
echo "$LOG"

extract_timestamp_us() {
    sed -n 's/.*TIMESTAMP=\([0-9][0-9]*\)us.*/\1/p'
}

echo
echo "===== CANCELLATION TIMESTAMP ====="

cancellation_us="$(
    grep 'EV=LIFECYCLE_REQUEST.*action=2' "$LOG" |
    head -1 |
    extract_timestamp_us
)"

if [[ -z "$cancellation_us" ]]; then
    echo "FAIL P21.1 cancellation request timestamp not found"
    exit 1
fi

echo "cancellation_us=$cancellation_us"

echo
echo "===== FIRST REPLAY DROP ====="

first_replay_drop_us="$(
    grep 'EV=DENM_REPLAY_DROP' "$LOG" |
    extract_timestamp_us |
    sort -n |
    head -1
)"

if [[ -z "$first_replay_drop_us" ]]; then
    echo "FAIL P21.1 CBF forwarding produced no replay-drop evidence"
    exit 1
fi

echo "first_replay_drop_us=$first_replay_drop_us"

echo
echo "===== CBF RUNTIME -> NS-3 TIMING ====="

if (( first_replay_drop_us >= cancellation_us )); then
    echo "FAIL P21.1 CBF forwarding remained dormant until/after cancellation"
    echo "first_replay_drop_us=$first_replay_drop_us"
    echo "cancellation_us=$cancellation_us"
    exit 1
fi

echo "PASS P21.1 CBF Runtime deadline propagated before cancellation"
echo "first_replay_drop_us=$first_replay_drop_us"
echo "cancellation_us=$cancellation_us"

echo
echo "P21_1_LIFECYCLE_ACCEPTANCE_EXIT=0"
