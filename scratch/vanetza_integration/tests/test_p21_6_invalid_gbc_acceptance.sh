#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BIN="$ROOT/build/scratch/ns3.47-denm_v3_invalid_gbc_robustness_test-default"
LOG="$ROOT/test-results/p21/p21_6_invalid_gbc.log"

fail() {
    echo "FAIL P21.6 $1"
    exit 1
}

pass() {
    echo "PASS P21.6 $1"
}

echo "============================================================"
echo "P21.6 — INVALID GBC ROBUSTNESS ACCEPTANCE"
echo "============================================================"

[[ -x "$BIN" ]] ||
    fail "invalid GBC robustness binary not found"

mkdir -p "$(dirname "$LOG")"

if ! "$BIN" >"$LOG" 2>&1; then
    cat "$LOG"
    fail "invalid GBC robustness executable failed"
fi

echo
echo "===== EXECUTION RESULT ====="

grep -Fq \
    'P21_6_INVALID_GBC_GREEN_EXIT=0' \
    "$LOG" ||
    fail "runtime test did not report GREEN_EXIT=0"

pass "runtime invalid GBC test reported GREEN_EXIT=0"

grep -Fq \
    'PASS invalid GBC latitude is rejected' \
    "$LOG" ||
    fail "invalid latitude rejection evidence missing"

grep -Fq \
    'PASS invalid GBC longitude is rejected' \
    "$LOG" ||
    fail "invalid longitude rejection evidence missing"

pass "latitude outside [-90,90] is rejected"
pass "longitude outside [-180,180] is rejected"

grep -Fq \
    'PASS invalid GBC marks PacketContext as FAIL' \
    "$LOG" ||
    fail "latitude PacketContext failure evidence missing"

grep -Fq \
    'PASS invalid GBC longitude marks PacketContext as FAIL' \
    "$LOG" ||
    fail "longitude PacketContext failure evidence missing"

grep -Fq \
    'PASS invalid GBC exposes invalid_gbc_area reason' \
    "$LOG" ||
    fail "latitude invalid_gbc_area reason missing"

grep -Fq \
    'PASS invalid GBC longitude exposes invalid_gbc_area reason' \
    "$LOG" ||
    fail "longitude invalid_gbc_area reason missing"

grep -Fq \
    'PASS invalid GBC does not advance to GEONET_TX' \
    "$LOG" ||
    fail "latitude GEONET_TX guard evidence missing"

grep -Fq \
    'PASS invalid GBC longitude does not advance to GEONET_TX' \
    "$LOG" ||
    fail "longitude GEONET_TX guard evidence missing"

pass "invalid GBC requests fail before GEONET_TX"

btp_fail_count="$(
    grep -Fc '[REASON=invalid_gbc_area]' "$LOG" || true
)"

[[ "$btp_fail_count" -eq 2 ]] ||
    fail "expected 2 invalid_gbc_area events, got $btp_fail_count"

pass "exactly two invalid_gbc_area BTP_TX_FAIL events observed"

if grep -Fq '[EV=GEONET_TX_BEGIN]' "$LOG"; then
    fail "invalid GBC request reached GEONET_TX_BEGIN"
fi

pass "no invalid GBC request reached GEONET_TX"

echo
echo "============================================================"
echo "P21_6_INVALID_GBC_ACCEPTANCE_EXIT=0"
echo "============================================================"
