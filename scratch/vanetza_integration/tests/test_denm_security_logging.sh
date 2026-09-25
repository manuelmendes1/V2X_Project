#!/usr/bin/env bash
set -u

if [ "$#" -ne 1 ]; then
    echo "USAGE: $0 <log_file>"
    exit 2
fi

LOG="$1"

fail=0

sign_begin=$(
    grep -c '\[STAGE=SIGN\].*\[MOD=SECURITY\]\[EV=SIGN_BEGIN\]' "$LOG" || true
)

sign_ok=$(
    grep -c '\[STAGE=SIGN\].*\[MOD=SECURITY\]\[EV=SIGN_OK\]' "$LOG" || true
)

denm_sign_marker=$(
    grep -c '\[STAGE=SIGN\].*\[MOD=SECURITY\]\[EV=SIGN_OK\].*DENM_V3' "$LOG" || true
)

denm_sign_invalid=$(
    grep '\[STAGE=SIGN\].*\[MOD=SECURITY\]\[EV=SIGN_OK\].*DENM_V3' "$LOG" \
        | grep -vc 'protocol_version=3 aid=37 signed=1' || true
)

real_signer=$(
    grep -c '\[SECURITY\] DENM V3 signer=REAL' "$LOG" || true
)

signer_tx=$(
    grep -c 'DENM_V3_SIGNER_TX.*contains_certificate=1' "$LOG" || true
)

echo "SIGN_BEGIN=$sign_begin"
echo "SIGN_OK=$sign_ok"
echo "DENM_V3_SIGN_OK=$denm_sign_marker"
echo "DENM_V3_SIGN_INVALID=$denm_sign_invalid"
echo "DENM_V3_REAL_SIGNER=$real_signer"
echo "DENM_V3_SIGNER_TX_WITH_CERTIFICATE=$signer_tx"

if [ "$sign_begin" -ne "$sign_ok" ]; then
    echo "FAIL: SIGN_BEGIN != SIGN_OK"
    fail=1
fi

if [ "$denm_sign_marker" -lt 1 ]; then
    echo "FAIL: no structured DENM V3 SIGN_OK events"
    fail=1
fi

if [ "$denm_sign_invalid" -ne 0 ]; then
    echo "FAIL: invalid DENM V3 SIGN_OK event detected"
    fail=1
fi

if [ "$real_signer" -lt 1 ]; then
    echo "FAIL: no REAL DENM V3 signer observed"
    fail=1
fi

if [ "$signer_tx" -lt 1 ]; then
    echo "FAIL: no DENM V3 signer TX event with embedded certificate"
    fail=1
fi

if [ "$fail" -eq 0 ]; then
    echo "PASS: DENM V3 security logging is valid"
fi

exit "$fail"
