#!/usr/bin/env bash
set -euo pipefail

RUNTIME_LOG="${1:-test-results/p21/p21_4_high_density_runtime_100.log}"
BRIDGE_LOG="${2:-test-results/p21/p21_4_bridge_clean.log}"
NS3_LOG="${3:-test-results/p21/p21_4_bridge_ns3_clean.log}"
PKI_DIR="${4:-$HOME/vanetza-pki-lab/validation-denm-pki}"

fail() {
    echo "FAIL P21.4 $*"
    exit 1
}

pass() {
    echo "PASS P21.4 $*"
}

require_file() {
    [[ -f "$1" ]] || fail "file not found: $1"
}

echo "============================================================"
echo "P21.4 — HIGH DENSITY + AUTOMATIC PROVISIONING + BRIDGE"
echo "============================================================"
echo "RUNTIME_LOG=$RUNTIME_LOG"
echo "BRIDGE_LOG=$BRIDGE_LOG"
echo "NS3_LOG=$NS3_LOG"
echo "PKI_DIR=$PKI_DIR"

require_file "$RUNTIME_LOG"
require_file "$BRIDGE_LOG"
require_file "$NS3_LOG"
[[ -d "$PKI_DIR" ]] || fail "PKI directory not found: $PKI_DIR"

echo
echo "===== HIGH-DENSITY RUNTIME ====="

grep -q '^VEHICLE_COUNT=100$' "$RUNTIME_LOG" \
    || fail "runtime vehicle count is not 100"

grep -q '^EXPECTED_SIGNER_COUNT=300$' "$RUNTIME_LOG" \
    || fail "runtime expected signer count is not 300"

grep -q '^UNIQUE_GLOBAL_SIGNER_COUNT=300$' "$RUNTIME_LOG" \
    || fail "runtime unique signer count is not 300"

grep -q \
    '^DENM_HIGH_DENSITY_V3_RUNTIME_ROTATION_EXIT=0$' \
    "$RUNTIME_LOG" \
    || fail "high-density runtime test did not return EXIT=0"

if grep -q '^FAIL ' "$RUNTIME_LOG"; then
    fail "runtime log contains FAIL entries"
fi

pass "runtime vehicle count = 100"
pass "runtime expected signer count = 300"
pass "runtime unique signer count = 300"
pass "high-density runtime test returned EXIT=0"
pass "runtime log contains no FAIL entries"

echo
echo "===== PKI PROVISIONING ====="

cert_count="$(
    find "$PKI_DIR" -maxdepth 1 -type f \
        -name 'veh*-ticket-*.cert' | wc -l
)"

key_count="$(
    find "$PKI_DIR" -maxdepth 1 -type f \
        -name 'veh*-ticket-*.pem' | wc -l
)"

[[ "$cert_count" -eq 300 ]] \
    || fail "PKI certificate count is $cert_count, expected 300"

[[ "$key_count" -eq 300 ]] \
    || fail "PKI key count is $key_count, expected 300"

pass "PKI certificates = 300"
pass "PKI keys = 300"

for i in $(seq 0 99); do
    for seq in 000 001 002; do
        cert="$PKI_DIR/veh${i}-ticket-${seq}.cert"
        key="$PKI_DIR/veh${i}-ticket-${seq}.pem"

        [[ -f "$cert" ]] \
            || fail "missing certificate: veh${i}-ticket-${seq}.cert"

        [[ -f "$key" ]] \
            || fail "missing key: veh${i}-ticket-${seq}.pem"
    done
done

pass "veh0..veh99 each have ticket-000/001/002 certificate"
pass "veh0..veh99 each have ticket-000/001/002 key"

echo
echo "===== BRIDGE / NS-3 CORRELATION ====="

python3 - "$BRIDGE_LOG" "$NS3_LOG" <<'PY'
import re
import sys

bridge_log = sys.argv[1]
ns3_log = sys.argv[2]

bridge_events = []
bridge_event_set = set()
bridge_vehicles = set()
ingest_events = []
ingest_set = set()

with open(bridge_log, encoding="utf-8", errors="replace") as f:
    for line in f:
        if "SUMO -> NS3 | " in line:
            payload = line.split("SUMO -> NS3 | ", 1)[1].strip()
            vehicle = payload.split(",", 1)[0]
            if re.fullmatch(r"veh[0-9]+", vehicle):
                bridge_vehicles.add(vehicle)

        marker = "DENM_EVENT | DENM_EVENT,"
        if marker not in line:
            continue

        payload = line.split(marker, 1)[1].strip()
        fields = payload.split(",")

        if len(fields) != 10:
            continue

        try:
            vehicle = fields[0]
            event = fields[1]
            trigger_us = int(fields[8])
        except (ValueError, IndexError):
            continue

        key = (vehicle, event, trigger_us)

        if key not in bridge_event_set:
            bridge_event_set.add(key)
            bridge_events.append(key)

with open(ns3_log, encoding="utf-8", errors="replace") as f:
    for line in f:
        if "[DENM_EVENT_INGEST]" not in line:
            continue

        vehicle = re.search(r"vehicle=([^\s]+)", line)
        event = re.search(r"event=([^\s]+)", line)
        trigger = re.search(r"trigger_us=([0-9]+)", line)

        if not (vehicle and event and trigger):
            continue

        key = (
            vehicle.group(1),
            event.group(1),
            int(trigger.group(1)),
        )

        if key not in ingest_set:
            ingest_set.add(key)
            ingest_events.append(key)

selected = bridge_events[:7]
selected_set = set(selected)

matched = selected_set & ingest_set
missing = selected_set - ingest_set
unexpected = ingest_set - bridge_event_set

print(f"bridge_observed_unique_vehicles={len(bridge_vehicles)}")
print(f"bridge_unique_events={len(bridge_event_set)}")
print(f"ns3_unique_ingest_events={len(ingest_set)}")
print(f"selected_events={len(selected_set)}")
print(f"matched={len(matched)}")
print(f"missing={len(missing)}")
print(f"unexpected={len(unexpected)}")

if len(bridge_vehicles) != 100:
    print(
        "FAIL: expected 100 unique SUMO vehicles at bridge, "
        f"got {len(bridge_vehicles)}"
    )
    sys.exit(1)

if len(selected_set) != 7:
    print("FAIL: fewer than 7 distinct bridge DENM events")
    sys.exit(1)

print()
print("===== SELECTED 7 =====")

for key in selected:
    status = "MATCH" if key in matched else "MISSING"
    print(
        f"{status} vehicle={key[0]} "
        f"event={key[1]} trigger_us={key[2]}"
    )

if len(matched) != 7:
    print("FAIL: selected bridge events are not all present at ns-3")
    sys.exit(1)

if missing:
    print("FAIL: missing bridge events:")
    for key in sorted(missing):
        print(f"  {key}")
    sys.exit(1)

if unexpected:
    print("FAIL: unexpected ns-3 ingest events:")
    for key in sorted(unexpected):
        print(f"  {key}")
    sys.exit(1)

print("PASS: 100 SUMO vehicles observed by bridge")
print("PASS: 7 bridge events matched at ns-3")
print("PASS: missing=0")
print("PASS: unexpected=0")
PY

echo
echo "===== DENM PIPELINE ====="

bridge_events="$(
    grep -ac 'DENM_EVENT | DENM_EVENT,' "$BRIDGE_LOG" || true
)"

ingest_events="$(
    grep -ac '\[DENM_EVENT_INGEST\]' "$NS3_LOG" || true
)"

gbc_requests="$(
    grep -ac '\[EV=GBC_REQUEST\]' "$NS3_LOG" || true
)"

deliveries="$(
    grep -ac '\[EV=DENM_DELIVER_OK\]' "$NS3_LOG" || true
)"

echo "bridge_denm_emit_count=$bridge_events"
echo "denm_event_ingest_count=$ingest_events"
echo "GBC_REQUEST_count=$gbc_requests"
echo "DENM_DELIVER_OK_count=$deliveries"

(( bridge_events >= 7 )) \
    || fail "bridge emitted fewer than 7 DENM_EVENT"

(( ingest_events >= 7 )) \
    || fail "ns-3 ingested fewer than 7 DENM_EVENT"

(( gbc_requests >= 1 )) \
    || fail "no GBC_REQUEST observed"

(( deliveries >= 1 )) \
    || fail "no DENM_DELIVER_OK observed"

pass "bridge emitted at least 7 DENM_EVENT"
pass "ns-3 ingested at least 7 DENM_EVENT"
pass "GBC_REQUEST observed"
pass "DENM_DELIVER_OK observed"

echo
echo "P21_4_HIGH_DENSITY_BRIDGE_ACCEPTANCE_EXIT=0"
