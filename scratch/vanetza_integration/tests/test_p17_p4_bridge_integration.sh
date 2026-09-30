#!/usr/bin/env bash
set -u

ROOT="$HOME/final/ns-3.47"

LOG="$ROOT/test-results/p17/p17_ns3.log"
BRIDGE_LOG="$ROOT/test-results/p17/p17_p4_bridge.log"

BRIDGE_DIR="$ROOT/../v2x_sumo/integration"
BRIDGE_SCRIPT="p4_bridge_high_density.py"

cd "$ROOT" || exit 1

DENM_PKI_DIR="$HOME/vanetza-pki-lab/validation-denm-pki"
DENM_ITS_START_TIME="${V2X_TEST_ITS_START_TIME:-2026-09-24T13:38:21}"
SIM_DURATION="${V2X_TEST_SIM_DURATION:-60}"

rm -f "$LOG" "$BRIDGE_LOG"

echo "===== P17 — P4 BRIDGE INTEGRATION ====="
echo "ROOT=$ROOT"
echo "BRIDGE=$BRIDGE_DIR/$BRIDGE_SCRIPT"
echo "LOG=$LOG"
echo "BRIDGE_LOG=$BRIDGE_LOG"

if ! command -v python3 >/dev/null 2>&1; then
    echo "FAIL: python3 is not available"
    echo "P17_P4_BRIDGE_INTEGRATION_EXIT=1"
    exit 1
fi

if ! python3 -c 'import traci' >/dev/null 2>&1; then
    echo "FAIL: Python traci module is not available"
    echo "P17_P4_BRIDGE_INTEGRATION_EXIT=1"
    exit 1
fi

if [ ! -f "$BRIDGE_DIR/$BRIDGE_SCRIPT" ]; then
    echo "FAIL: P4 bridge not found: $BRIDGE_DIR/$BRIDGE_SCRIPT"
    echo "P17_P4_BRIDGE_INTEGRATION_EXIT=1"
    exit 1
fi

if [ ! -x "$ROOT/build/scratch_v2x_sumo" ]; then
    echo "FAIL: ns-3 simulator not found: $ROOT/build/scratch_v2x_sumo"
    echo "P17_P4_BRIDGE_INTEGRATION_EXIT=1"
    exit 1
fi

if ss -lun 2>/dev/null | grep -qE '(:|\.)7400[[:space:]]'; then
    echo "FAIL: UDP port 7400 is already in use"
    echo "P17_P4_BRIDGE_INTEGRATION_EXIT=1"
    exit 1
fi

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

./build/scratch_v2x_sumo \
    --enableDenm=1 \
    --denmRadius=1000 \
    --denmPkiDir="$DENM_PKI_DIR" \
    --itsStartTime="$DENM_ITS_START_TIME" \
    --simDuration="$SIM_DURATION" \
    > "$LOG" 2>&1 &

ns3_pid=$!

port_ready=0

for _ in $(seq 1 100); do
    if ss -lun 2>/dev/null | grep -qE '(:|\.)7400[[:space:]]'; then
        port_ready=1
        break
    fi

    if ! kill -0 "$ns3_pid" 2>/dev/null; then
        wait "$ns3_pid"
        ns3_rc=$?

        echo "FAIL: ns-3 exited before UDP 7400 was ready"
        echo "ns3_exit=$ns3_rc"
        echo "P17_P4_BRIDGE_INTEGRATION_EXIT=1"
        exit 1
    fi

    sleep 0.1
done

if [ "$port_ready" -ne 1 ]; then
    kill "$ns3_pid" 2>/dev/null || true
    wait "$ns3_pid" 2>/dev/null || true

    echo "FAIL: ns-3 did not bind UDP port 7400"
    echo "P17_P4_BRIDGE_INTEGRATION_EXIT=1"
    exit 1
fi

(
    cd "$BRIDGE_DIR" || exit 1
    exec python3 "$BRIDGE_SCRIPT"
) > "$BRIDGE_LOG" 2>&1 &

bridge_pid=$!

wait "$ns3_pid"
ns3_rc=$?

if [ "$ns3_rc" -ne 0 ]; then
    echo "FAIL: ns-3 exit_code=$ns3_rc"
    echo "P17_P4_BRIDGE_INTEGRATION_EXIT=1"
    exit 1
fi

if kill -0 "$bridge_pid" 2>/dev/null; then
    kill -INT "$bridge_pid" 2>/dev/null || true
    wait "$bridge_pid" 2>/dev/null || true
fi

python3 - "$BRIDGE_LOG" "$LOG" <<'PY'
import re
import sys

bridge_log = sys.argv[1]
ns3_log = sys.argv[2]

emitted = []
ingested = []
deliveries = []

with open(bridge_log, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        if "DENM_EVENT | DENM_EVENT," not in line:
            continue

        m = re.search(
            r"DENM_EVENT \| DENM_EVENT,"
            r"([^,]+),"
            r"([^,]+),"
            r"([^,]+),"
            r"([^,]+),"
            r"([^,]+),"
            r"([^,]+),"
            r"([^,]+),"
            r"([^,]+),"
            r"([^,]+),"
            r"([^\s]+)",
            line,
        )

        if not m:
            continue

        emitted.append(
            {
                "vehicle": m.group(1),
                "event": m.group(2),
                "type": m.group(3),
                "trigger": int(m.group(9)),
                "reason": m.group(10),
            }
        )

with open(ns3_log, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        if "[DENM_EVENT_INGEST]" in line:
            vehicle = re.search(r"vehicle=([^\s]+)", line)
            event = re.search(r"event=([^\s]+)", line)
            trigger = re.search(r"trigger_us=([0-9]+)", line)

            if vehicle and event and trigger:
                ingested.append(
                    {
                        "vehicle": vehicle.group(1),
                        "event": event.group(1),
                        "trigger": int(trigger.group(1)),
                    }
                )

        if "[EV=DENM_DELIVER_OK]" in line:
            deliveries.append(line)

print(f"bridge_denm_emit_count={len(emitted)}")
print(f"denm_event_ingest_count={len(ingested)}")
print(f"denm_deliver_ok_count={len(deliveries)}")

if not emitted:
    print("FAIL: P4 bridge emitted no DENM_EVENT")
    sys.exit(1)

if not ingested:
    print("FAIL: no DENM_EVENT reached ns-3 ingestion")
    sys.exit(1)

emitted_keys = {
    (x["vehicle"], x["event"], x["trigger"])
    for x in emitted
}

ingested_keys = {
    (x["vehicle"], x["event"], x["trigger"])
    for x in ingested
}

common = emitted_keys & ingested_keys

print("matched_bridge_ingest_events=")

for key in sorted(common):
    print(
        f"  vehicle={key[0]} "
        f"event={key[1]} "
        f"trigger_us={key[2]}"
    )

if not common:
    print(
        "FAIL: bridge DENM_EVENT identifiers/timestamps "
        "were not observed at ns-3 ingestion"
    )
    sys.exit(1)

scheduler_markers = [
    "EV=GBC_REQUEST",
    "DENM_NEW",
    "DENM_LIFECYCLE",
    "EV=DENM_DELIVER_OK",
]

marker_counts = {
    marker: sum(marker in line for line in open(
        ns3_log,
        "r",
        encoding="utf-8",
        errors="replace",
    ))
    for marker in scheduler_markers
}

for marker, count in marker_counts.items():
    print(f"{marker}_count={count}")

if marker_counts["EV=GBC_REQUEST"] < 1:
    print("FAIL: no DENM GBC_REQUEST observed after bridge ingestion")
    sys.exit(1)

if marker_counts["EV=DENM_DELIVER_OK"] < 1:
    print("FAIL: no successful DENM delivery observed")
    sys.exit(1)

print(
    "PASS: P4 bridge DENM_EVENT crossed UDP 7400, "
    "was ingested by ns-3, entered the DENM transmission path, "
    "and produced at least one successful delivery"
)

sys.exit(0)
PY

rc=$?

echo
echo "===== P17 RESULT ====="
echo "P17_P4_BRIDGE_INTEGRATION_EXIT=$rc"

exit "$rc"
