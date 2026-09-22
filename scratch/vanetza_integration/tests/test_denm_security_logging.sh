#!/usr/bin/env bash
set -u

LOG="${1:-output_log_etapa_8_10.txt}"

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

echo "SIGN_BEGIN=$sign_begin"
echo "SIGN_OK=$sign_ok"
echo "DENM_V3_SIGN_OK=$denm_sign_marker"

if [ "$denm_sign_marker" -ne 4 ]; then
    echo "FAIL: expected 4 structured DENM V3 SIGN_OK events"
    fail=1
fi

exit "$fail"
