#!/usr/bin/env bash

set -u

LOG="${1:-}"
RUN_EXIT="${2:-0}"

if [[ -z "$LOG" ]]; then
    echo "USAGE: $0 <log_file> [run_exit]"
    exit 2
fi

if [[ ! -f "$LOG" ]]; then
    echo "[FAIL] log file not found: $LOG"
    exit 2
fi

failures=0

count_event() {
    local ev="$1"
    grep -cF "[EV=$ev]" "$LOG" 2>/dev/null || true
}

count_pattern() {
    local pattern="$1"
    grep -Ec "$pattern" "$LOG" 2>/dev/null || true
}

require_positive_event() {
    local ev="$1"
    local count
    count="$(count_event "$ev")"

    if [[ "$count" -gt 0 ]]; then
        printf '[PASS] %-24s > 0 (%s)\n' "$ev" "$count"
    else
        printf '[FAIL] %-24s > 0 (%s)\n' "$ev" "$count"
        failures=$((failures + 1))
    fi
}

require_same_count() {
    local label="$1"
    shift

    local first_ev="$1"
    shift

    local expected
    expected="$(count_event "$first_ev")"

    local ev
    local actual

    for ev in "$@"; do
        actual="$(count_event "$ev")"

        if [[ "$actual" -eq "$expected" ]]; then
            printf '[PASS] %-24s = %s\n' "$ev" "$actual"
        else
            printf '[FAIL] %-24s expected=%s actual=%s\n' \
                "$ev" "$expected" "$actual"
            failures=$((failures + 1))
        fi
    done

    printf '[INFO] %s baseline event=%s count=%s\n' \
        "$label" "$first_ev" "$expected"
}

echo "===== ETAPA 8.13 — CAM REGRESSION ACCEPTANCE ====="
echo "LOG=$LOG"
echo "RUN_EXIT=$RUN_EXIT"

echo
echo "===== RUN STATUS ====="

if [[ "$RUN_EXIT" -eq 0 ]]; then
    echo "[PASS] simulation exit code == 0"
else
    echo "[FAIL] simulation exit code != 0 ($RUN_EXIT)"
    failures=$((failures + 1))
fi

echo
echo "===== CAM TX PIPELINE ====="

require_same_count "TX chain" \
    BTP_TX_BEGIN \
    BTP_TX_OK \
    GEONET_TX_BEGIN \
    GEONET_TX_OK \
    ACCESS_TX_BEGIN \
    ACCESS_TX_OK \
    SEND_BEGIN \
    SEND_OK \
    TX_COMPLETE

echo
echo "===== CAM RX PIPELINE ====="

require_same_count "RX chain" \
    RAW_RX \
    ACCESS_RX_BEGIN \
    ACCESS_RX_OK \
    GEONET_RX_BEGIN \
    GEONET_RX_OK \
    BTP_RX_BEGIN \
    BTP_RX_OK \
    VERIFY_BEGIN \
    VERIFY_OK \
    DECODE_OK \
    REPLAY_OK \
    RECEPTION_BEGIN \
    RECEPTION_OK \
    DELIVER_BEGIN \
    DELIVER_OK

echo
echo "===== CAM IDENTITY / TRANSPORT ====="

cam_btp_tx="$(count_pattern '\[EV=BTP_TX_OK\].*port=2001')"

if [[ "$cam_btp_tx" -gt 0 ]]; then
    echo "[PASS] CAM BTP TX uses port=2001 ($cam_btp_tx)"
else
    echo "[FAIL] CAM BTP TX port=2001 not observed"
    failures=$((failures + 1))
fi

cam_gn_tx="$(count_pattern '\[EV=GEONET_TX_OK\].*transport=SHB.*profile=ITS_G5.*aid=CA')"

if [[ "$cam_gn_tx" -gt 0 ]]; then
    echo "[PASS] CAM GeoNetworking TX uses SHB / ITS_G5 / AID=CA ($cam_gn_tx)"
else
    echo "[FAIL] CAM GeoNetworking TX identity not observed"
    failures=$((failures + 1))
fi

cam_btp_rx="$(count_pattern '\[EV=BTP_RX_OK\].*port=2001')"

if [[ "$cam_btp_rx" -gt 0 ]]; then
    echo "[PASS] CAM BTP RX uses port=2001 ($cam_btp_rx)"
else
    echo "[FAIL] CAM BTP RX port=2001 not observed"
    failures=$((failures + 1))
fi

echo
echo "===== DENM ABSENCE ====="

denm_events="$(grep -Ec \
    '\[EV=[A-Z0-9_]*DENM[A-Z0-9_]*\]|\[EV=GBC_REQUEST\]|\[EV=GBC_REQUEST_OK\]' \
    "$LOG" 2>/dev/null || true)"

if [[ "$denm_events" -eq 0 ]]; then
    echo "[PASS] no DENM/GBC events observed ($denm_events)"
else
    echo "[FAIL] DENM/GBC events observed ($denm_events)"
    grep -E \
        '\[EV=[A-Z0-9_]*DENM[A-Z0-9_]*\]|\[EV=GBC_REQUEST\]|\[EV=GBC_REQUEST_OK\]' \
        "$LOG" |
        head -20
    failures=$((failures + 1))
fi

echo
echo "===== VEHICLE OBSERVATION ====="

vehicles="$(
    grep -oE '\[VEH=[^]]+\]' "$LOG" 2>/dev/null |
    sort -u |
    grep -v '^\[VEH=-\]$' |
    tr '\n' ' ' || true
)"

if [[ -n "$vehicles" ]]; then
    echo "[PASS] vehicle observations present: $vehicles"
else
    echo "[FAIL] no vehicle observations found"
    failures=$((failures + 1))
fi

echo
echo "===== FINAL RESULT ====="

if [[ "$failures" -eq 0 ]]; then
    echo "RESULTADO 8.13: PASS"
    echo "ACCEPTANCE_EXIT=0"
    exit 0
else
    echo "RESULTADO 8.13: FAIL"
    echo "ACCEPTANCE_EXIT=1"
    exit 1
fi
