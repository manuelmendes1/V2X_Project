#!/usr/bin/env bash
set -u

ROOT="$HOME/final/ns-3.47"
LOG="$ROOT/output_log_denm_invalid_signature_test.txt"
BRIDGE_LOG="$ROOT/output_log_denm_invalid_signature_bridge.txt"
BRIDGE_DIR="$ROOT/../v2x_sumo/integration"
BRIDGE_SCRIPT="v2x_bridge_15.py"

cd "$ROOT" || exit 1

unset V2X_DENM_PKI_DIR
export V2X_DENM_TAMPER=1
unset V2X_DENM_DUPLICATE

DENM_PKI_DIR="$HOME/vanetza-pki-lab/validation-denm-pki"
DENM_ITS_START_TIME="${V2X_TEST_ITS_START_TIME:-2026-09-24T13:38:21}"

if ! command -v python3 >/dev/null 2>&1; then
    echo "FAIL: python3 is not available"
    exit 1
fi

if ! python3 -c 'import traci' >/dev/null 2>&1; then
    echo "FAIL: Python traci module is not available"
    exit 1
fi

if [ ! -f "$BRIDGE_DIR/$BRIDGE_SCRIPT" ]; then
    echo "FAIL: SUMO bridge not found: $BRIDGE_DIR/$BRIDGE_SCRIPT"
    exit 1
fi

if ss -lun 2>/dev/null | grep -qE '(:|\.)7400[[:space:]]'; then
    echo "FAIL: UDP port 7400 is already in use"
    exit 1
fi

./build/scratch_v2x_sumo \
    --enableDenm=1 \
    --denmRadius=1000 \
    --denmPkiDir="$DENM_PKI_DIR" \
    --itsStartTime="$DENM_ITS_START_TIME" \
    --simDuration="${V2X_TEST_SIM_DURATION:-60}" \
    > "$LOG" 2>&1 &

ns3_pid=$!

cleanup()
{
    rc=$?

    if [ -n "${bridge_pid:-}" ] &&
       kill -0 "$bridge_pid" 2>/dev/null; then
        kill -INT "$bridge_pid" 2>/dev/null || true
        wait "$bridge_pid" 2>/dev/null || true
    fi

    exit "$rc"
}

trap cleanup EXIT INT TERM

port_ready=0

for _ in $(seq 1 50); do
    if ss -lun 2>/dev/null | grep -qE '(:|\.)7400[[:space:]]'; then
        port_ready=1
        break
    fi

    if ! kill -0 "$ns3_pid" 2>/dev/null; then
        wait "$ns3_pid"
        run_exit=$?
        echo "FAIL: simulator exited before UDP port 7400 was ready (exit_code=$run_exit)"
        exit 1
    fi

    sleep 0.1
done

if [ "$port_ready" -ne 1 ]; then
    kill "$ns3_pid" 2>/dev/null || true
    wait "$ns3_pid" 2>/dev/null || true
    echo "FAIL: ns-3 did not bind UDP port 7400"
    exit 1
fi

(
    cd "$BRIDGE_DIR" || exit 1
    exec python3 "$BRIDGE_SCRIPT"
) > "$BRIDGE_LOG" 2>&1 &

bridge_pid=$!

wait "$ns3_pid"
run_exit=$?

if [ "$run_exit" -ne 0 ]; then
    echo "FAIL: simulator exit_code=$run_exit"
    exit 1
fi

if kill -0 "$bridge_pid" 2>/dev/null; then
    kill -INT "$bridge_pid" 2>/dev/null || true
    wait "$bridge_pid" 2>/dev/null || true
fi

sumo_to_ns3_count=$(
    grep -c 'SUMO -> NS3' "$BRIDGE_LOG" || true
)

if [ "$sumo_to_ns3_count" -lt 1 ]; then
    echo "FAIL: SUMO bridge produced no SUMO -> NS3 vehicle state"
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

echo "sumo_to_ns3_count=$sumo_to_ns3_count"
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
