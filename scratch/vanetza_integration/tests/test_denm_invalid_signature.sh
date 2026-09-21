#!/usr/bin/env bash
set -u

ROOT="$HOME/final/ns-3.47"
LOG="$ROOT/output_log_denm_invalid_signature_test.txt"

cd "$ROOT" || exit 1

export V2X_DENM_PKI_DIR="$HOME/vanetza-pki-lab/validation-denm-pki"
export V2X_DENM_TAMPER=1

./build/scratch_v2x_sumo \
  --enableDenm=1 \
  --denmRadius=1000 \
  --simDuration="${V2X_TEST_SIM_DURATION:-60}" \
  > "$LOG" 2>&1

run_exit=$?

if [ "$run_exit" -ne 0 ]; then
    echo "FAIL: simulator exit_code=$run_exit"
    exit 1
fi

tamper_count=$(
    grep -c '\[DENM_V3_TAMPER\]' "$LOG" || true
)

denm_rx_count=$(
    grep -c 'EV=ACCESS_RX_OK.*bytes=339' "$LOG" || true
)

verify_entry_count=$(
    grep -c '\[DENM_V3_VERIFY_ENTRY\] aid=37' "$LOG" || true
)

verify_success_count=$(
    grep -c \
        '\[DENM_V3_VERIFY\].*aid=37.*report=SUCCESS.*cryptographic=PASS' \
        "$LOG" || true
)

false_signature_count=$(
    grep -c \
        '\[DENM_V3_VERIFY\].*aid=37.*report=FALSE_SIGNATURE.*cryptographic=FAIL' \
        "$LOG" || true
)

dispatch_count=$(
    grep -c 'EV=DENM_RX_DISPATCH.*aid=37' "$LOG" || true
)

decode_count=$(
    grep -c 'EV=DENM_DECODE_OK' "$LOG" || true
)

replay_count=$(
    grep -c 'EV=DENM_REPLAY_OK' "$LOG" || true
)

reception_count=$(
    grep -c 'EV=DENM_RECEPTION_OK' "$LOG" || true
)

deliver_count=$(
    grep -c 'EV=DENM_DELIVER_OK' "$LOG" || true
)

echo "tamper_count=$tamper_count"
echo "denm_rx_count=$denm_rx_count"
echo "verify_entry_count=$verify_entry_count"
echo "verify_success_count=$verify_success_count"
echo "false_signature_count=$false_signature_count"
echo "dispatch_count=$dispatch_count"
echo "decode_count=$decode_count"
echo "replay_count=$replay_count"
echo "reception_count=$reception_count"
echo "deliver_count=$deliver_count"

if [ "$tamper_count" -lt 1 ]; then
    echo "FAIL: no DENM V3 tamper event"
    exit 1
fi

if [ "$denm_rx_count" -lt 1 ]; then
    echo "FAIL: no tampered DENM reached ACCESS_RX"
    exit 1
fi

if [ "$verify_entry_count" -lt 1 ]; then
    echo "FAIL: no DENM AID 37 verification observed"
    exit 1
fi

if [ "$false_signature_count" -lt 1 ]; then
    echo "FAIL: no DENM V3 FALSE_SIGNATURE rejection observed"
    exit 1
fi

if [ "$verify_success_count" -ne 0 ]; then
    echo "FAIL: tampered DENM was cryptographically accepted"
    exit 1
fi

if [ "$dispatch_count" -ne 0 ]; then
    echo "FAIL: tampered DENM reached DENM_RX_DISPATCH"
    exit 1
fi

if [ "$decode_count" -ne 0 ]; then
    echo "FAIL: tampered DENM reached DECODE"
    exit 1
fi

if [ "$replay_count" -ne 0 ]; then
    echo "FAIL: tampered DENM reached REPLAY"
    exit 1
fi

if [ "$reception_count" -ne 0 ]; then
    echo "FAIL: tampered DENM reached RECEPTION"
    exit 1
fi

if [ "$deliver_count" -ne 0 ]; then
    echo "FAIL: tampered DENM reached DELIVER"
    exit 1
fi

echo "PASS: invalid DENM V3 signature was rejected by SecurityEntity before application delivery"
