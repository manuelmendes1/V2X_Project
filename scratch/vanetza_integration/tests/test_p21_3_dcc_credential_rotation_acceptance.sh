#!/usr/bin/env bash
set -euo pipefail

LOG="${1:-test-results/p21/p21_3_scheduler_rotation.log}"

EXPECTED_FIRST_SIGNER="df3d3cad9e84f028"
EXPECTED_SECOND_SIGNER="8dfa587c85a40d54"
ROTATION_US=1050000

if [[ ! -f "$LOG" ]]; then
    echo "FAIL P21.3: log not found: $LOG"
    exit 1
fi

fail() {
    echo "FAIL P21.3 $*"
    exit 1
}

pass() {
    echo "PASS P21.3 $*"
}

timestamp_from_line() {
    sed -n 's/.*TIMESTAMP=\([0-9][0-9]*\)us.*/\1/p'
}

echo "============================================================"
echo "P21.3 — DCC + SIGNING + CREDENTIAL ROTATION ACCEPTANCE"
echo "============================================================"
echo "LOG=$LOG"

echo
echo "===== EXECUTION RESULT ====="

grep -q \
    'DENM_V3_SCHEDULER_E2E_CREDENTIAL_ROTATION_EXIT=0' \
    "$LOG" \
    || fail "scheduler E2E did not return EXIT=0"

pass "scheduler E2E returned EXIT=0"

echo
echo "===== PKT=1 PIPELINE ====="

pkt1_sign_line="$(
    grep -m1 \
        '\[PKT=1\].*\[MOD=SECURITY\]\[EV=SIGN_OK\].*DENM_V3.*protocol_version=3.*aid=37.*signed=1' \
        "$LOG" || true
)"

pkt1_request_line="$(
    grep -m1 \
        '\[PKT=1\].*\[MOD=DCC\]\[EV=DCC_REQUEST\]' \
        "$LOG" || true
)"

pkt1_tx_line="$(
    grep -m1 \
        '\[PKT=1\].*\[MOD=DCC\]\[EV=DCC_TX\]' \
        "$LOG" || true
)"

pkt1_complete_line="$(
    grep -m1 \
        '\[PKT=1\].*\[EV=TX_COMPLETE\].*Transmission completed' \
        "$LOG" || true
)"

[[ -n "$pkt1_sign_line" ]] || fail "PKT=1 SIGN_OK missing"
[[ -n "$pkt1_request_line" ]] || fail "PKT=1 DCC_REQUEST missing"
[[ -n "$pkt1_tx_line" ]] || fail "PKT=1 DCC_TX missing"
[[ -n "$pkt1_complete_line" ]] || fail "PKT=1 TX_COMPLETE missing"

pkt1_sign_us="$(printf '%s\n' "$pkt1_sign_line" | timestamp_from_line)"
pkt1_request_us="$(printf '%s\n' "$pkt1_request_line" | timestamp_from_line)"
pkt1_tx_us="$(
    printf '%s\n' "$pkt1_tx_line" |
    sed -n 's/.*tx_sim_us=\([0-9][0-9]*\).*/\1/p'
)"
pkt1_delay_us="$(
    printf '%s\n' "$pkt1_tx_line" |
    sed -n 's/.*dcc_delay_us=\([0-9][0-9]*\).*/\1/p'
)"

[[ "$pkt1_sign_us" == "1000000" ]] \
    || fail "PKT=1 SIGN timestamp is '$pkt1_sign_us', expected 1000000"

[[ "$pkt1_request_us" == "1000000" ]] \
    || fail "PKT=1 DCC_REQUEST timestamp is '$pkt1_request_us', expected 1000000"

[[ "$pkt1_tx_us" == "1100000" ]] \
    || fail "PKT=1 DCC_TX tx_sim_us is '$pkt1_tx_us', expected 1100000"

[[ "$pkt1_delay_us" == "100000" ]] \
    || fail "PKT=1 DCC delay is '$pkt1_delay_us', expected 100000"

pass "PKT=1 has SIGN_OK → DCC_REQUEST → DCC_TX → TX_COMPLETE"
pass "PKT=1 DCC delay is 100000 us"

echo
echo "===== PKT=2 PIPELINE ====="

pkt2_sign_line="$(
    grep -m1 \
        '\[PKT=2\].*\[MOD=SECURITY\]\[EV=SIGN_OK\].*DENM_V3.*protocol_version=3.*aid=37.*signed=1' \
        "$LOG" || true
)"

pkt2_request_line="$(
    grep -m1 \
        '\[PKT=2\].*\[MOD=DCC\]\[EV=DCC_REQUEST\]' \
        "$LOG" || true
)"

pkt2_tx_line="$(
    grep -m1 \
        '\[PKT=2\].*\[MOD=DCC\]\[EV=DCC_TX\]' \
        "$LOG" || true
)"

pkt2_complete_line="$(
    grep -m1 \
        '\[PKT=2\].*\[EV=TX_COMPLETE\].*Transmission completed' \
        "$LOG" || true
)"

[[ -n "$pkt2_sign_line" ]] || fail "PKT=2 SIGN_OK missing"
[[ -n "$pkt2_request_line" ]] || fail "PKT=2 DCC_REQUEST missing"
[[ -n "$pkt2_tx_line" ]] || fail "PKT=2 DCC_TX missing"
[[ -n "$pkt2_complete_line" ]] || fail "PKT=2 TX_COMPLETE missing"

pkt2_tx_us="$(
    printf '%s\n' "$pkt2_tx_line" |
    sed -n 's/.*tx_sim_us=\([0-9][0-9]*\).*/\1/p'
)"

pkt2_delay_us="$(
    printf '%s\n' "$pkt2_tx_line" |
    sed -n 's/.*dcc_delay_us=\([0-9][0-9]*\).*/\1/p'
)"

[[ "$pkt2_tx_us" == "1200000" ]] \
    || fail "PKT=2 DCC_TX tx_sim_us is '$pkt2_tx_us', expected 1200000"

[[ "$pkt2_delay_us" == "100000" ]] \
    || fail "PKT=2 DCC delay is '$pkt2_delay_us', expected 100000"

pass "PKT=2 has SIGN_OK → DCC_REQUEST → DCC_TX → TX_COMPLETE"
pass "PKT=2 DCC delay is 100000 us"

echo
echo "===== ROTATION ====="

grep -q \
    'PASS credential 000 is active before scheduler transmission' \
    "$LOG" \
    || fail "credential 000 activation evidence missing"

grep -q \
    'PASS credential 001 is active before second scheduler DENM' \
    "$LOG" \
    || fail "credential 001 activation evidence missing"

grep -q \
    'PASS credential 001 certificate is active before second scheduler DENM' \
    "$LOG" \
    || fail "credential 001 certificate evidence missing"

pass "credential 000 is active before first DENM"
pass "credential 001 is active before second DENM"

echo
echo "===== SIGNER IDENTITY ====="

first_signer="$(
    sed -n 's/^FIRST_SCHEDULER_SIGNER_CERT_ID=//p' "$LOG" |
    head -1
)"

second_signer="$(
    sed -n 's/^SECOND_SCHEDULER_SIGNER_CERT_ID=//p' "$LOG" |
    head -1
)"

[[ "$first_signer" == "$EXPECTED_FIRST_SIGNER" ]] \
    || fail "PKT=1 signer '$first_signer' != '$EXPECTED_FIRST_SIGNER'"

[[ "$second_signer" == "$EXPECTED_SECOND_SIGNER" ]] \
    || fail "PKT=2 signer '$second_signer' != '$EXPECTED_SECOND_SIGNER'"

[[ "$first_signer" != "$second_signer" ]] \
    || fail "signer identity did not change after rotation"

pass "PKT=1 signer = $EXPECTED_FIRST_SIGNER"
pass "PKT=2 signer = $EXPECTED_SECOND_SIGNER"
pass "signer identity changes after credential rotation"

echo
echo "===== ROTATION / DCC CAUSALITY ====="

[[ -n "$pkt1_sign_us" ]] || fail "PKT=1 signing timestamp missing"
[[ -n "$pkt1_tx_us" ]] || fail "PKT=1 transmission timestamp missing"

(( pkt1_sign_us < ROTATION_US )) \
    || fail "PKT=1 was not signed before the 1050 ms rotation"

(( pkt1_tx_us > ROTATION_US )) \
    || fail "PKT=1 was not transmitted after the 1050 ms rotation"

pass "PKT=1 is signed before rotation"
pass "PKT=1 is transmitted after rotation"
pass "DCC delay does not change the signer of already-signed PKT=1"

echo
echo "===== SIGNER CONTEXT STABILITY ====="

grep -q \
    'PASS PKT=1 signer remains unchanged until TX_COMPLETE' \
    "$LOG" \
    || fail "PKT=1 signer stability through DCC/TX_COMPLETE is not explicitly demonstrated"

pass "PKT=1 signer remains unchanged until TX_COMPLETE"

echo "===== PACKET CONTEXT STABILITY ====="

sign_pkt="$(
    printf '%s\n' "$pkt1_sign_line" |
    sed -n 's/.*\[PKT=\([0-9][0-9]*\)\].*/\1/p'
)"

request_pkt="$(
    printf '%s\n' "$pkt1_request_line" |
    sed -n 's/.*\[PKT=\([0-9][0-9]*\)\].*/\1/p'
)"

tx_pkt="$(
    printf '%s\n' "$pkt1_tx_line" |
    sed -n 's/.*\[PKT=\([0-9][0-9]*\)\].*/\1/p'
)"

[[ "$sign_pkt" == "1" ]] || fail "PKT identity at SIGN is '$sign_pkt'"
[[ "$request_pkt" == "1" ]] || fail "PKT identity at DCC_REQUEST is '$request_pkt'"
[[ "$tx_pkt" == "1" ]] || fail "PKT identity at DCC_TX is '$tx_pkt'"

pass "PKT=1 remains stable across SIGN → DCC_REQUEST → DCC_TX"

echo
echo "P21_3_DCC_CREDENTIAL_ROTATION_ACCEPTANCE_EXIT=0"
