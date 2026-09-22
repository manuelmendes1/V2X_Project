#!/usr/bin/env bash
set -u

ROOT="$HOME/final/ns-3.47"
LOG="$ROOT/output_log_denm_replay_protection_test.txt"
BRIDGE_LOG="$ROOT/output_log_denm_replay_protection_bridge.txt"
BRIDGE_DIR="$ROOT/../v2x_sumo/integration"
BRIDGE_SCRIPT="v2x_bridge_15.py"

cd "$ROOT" || exit 1

unset V2X_DENM_TAMPER
export V2X_DENM_DUPLICATE=1
export V2X_DENM_PKI_DIR="$HOME/vanetza-pki-lab/validation-denm-pki"

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

python3 - "$LOG" "$sumo_to_ns3_count" <<'PY'
import re
import sys

log_path = sys.argv[1]
sumo_to_ns3_count = int(sys.argv[2])

replay_ok = []
replay_drop = []
deliver = []
verify_success = 0
duplicate_count = 0
denm_rx_count = 0

with open(log_path, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        if "DENM_V3_DUPLICATE" in line and "aid=37" in line:
            duplicate_count += 1

        if "EV=ACCESS_RX_OK" in line and "bytes=339" in line:
            denm_rx_count += 1

        if (
            "DENM_V3_VERIFY" in line
            and "aid=37" in line
            and "report=SUCCESS" in line
            and "cryptographic=PASS" in line
        ):
            verify_success += 1

        if "EV=DENM_REPLAY_OK" in line:
            receiver = re.search(r"\[VEH=([^\]]+)\]", line)
            origin = re.search(r"origin_station=(\d+)", line)
            seq = re.search(r"action_seq=(\d+)", line)
            pkt = re.search(r"\[PKT=(\d+)\]", line)

            if receiver and origin and seq and pkt:
                replay_ok.append(
                    (
                        receiver.group(1),
                        int(origin.group(1)),
                        int(seq.group(1)),
                        int(pkt.group(1)),
                    )
                )

        if "EV=DENM_REPLAY_DROP" in line:
            receiver = re.search(r"\[VEH=([^\]]+)\]", line)
            origin = re.search(r"origin_station=(\d+)", line)
            seq = re.search(r"action_seq=(\d+)", line)
            pkt = re.search(r"\[PKT=(\d+)\]", line)

            if receiver and origin and seq and pkt:
                replay_drop.append(
                    (
                        receiver.group(1),
                        int(origin.group(1)),
                        int(seq.group(1)),
                        int(pkt.group(1)),
                    )
                )

        if "EV=DENM_DELIVER_OK" in line:
            pkt = re.search(r"\[PKT=(\d+)\]", line)
            if pkt:
                deliver.append(int(pkt.group(1)))

print(f"sumo_to_ns3_count={sumo_to_ns3_count}")
print(f"duplicate_count={duplicate_count}")
print(f"denm_rx_count={denm_rx_count}")
print(f"verify_success_count={verify_success}")
print(f"replay_ok_count={len(replay_ok)}")
print(f"replay_drop_count={len(replay_drop)}")
print(f"deliver_count={len(deliver)}")

if duplicate_count < 1:
    print("FAIL: no deterministic DENM duplicate was transmitted")
    sys.exit(1)

if denm_rx_count < 1:
    print("FAIL: no DENM reached ACCESS_RX")
    sys.exit(1)

if verify_success < 1:
    print("FAIL: no valid DENM V3 verification observed")
    sys.exit(1)

if not replay_ok:
    print("FAIL: no DENM_REPLAY_OK observed")
    sys.exit(1)

if not replay_drop:
    print("FAIL: no DENM_REPLAY_DROP observed")
    sys.exit(1)

accepted_keys = {
    (receiver, origin, seq)
    for receiver, origin, seq, _ in replay_ok
}

dropped_keys = {
    (receiver, origin, seq)
    for receiver, origin, seq, _ in replay_drop
}

common_keys = accepted_keys & dropped_keys

if not common_keys:
    print(
        "FAIL: no receiver received the same "
        "ActionID first as OK and later as DROP"
    )
    sys.exit(1)

for receiver, origin, seq, pkt in replay_drop:
    if pkt in deliver:
        print(
            f"FAIL: replay-dropped PKT={pkt} reached DELIVER"
        )
        sys.exit(1)

print("common_replay_keys=")
for key in sorted(common_keys):
    print(
        f"  receiver={key[0]} "
        f"origin_station={key[1]} "
        f"action_seq={key[2]}"
    )

print(
    "PASS: DENM replay was accepted once per receiver and "
    "subsequent identical ActionID/payload copies were rejected "
    "before delivery"
)
PY
