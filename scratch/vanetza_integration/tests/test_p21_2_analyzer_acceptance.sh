#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"

LOG="${1:-test-results/p21/p21_4_bridge_ns3_clean.log}"
JSON="/tmp/p21_2_analyzer_acceptance.json"

echo "============================================================"
echo "P21.2 — V2X ANALYZER ACCEPTANCE"
echo "============================================================"
echo "LOG=$LOG"

fail=0

check() {
    if "$@"; then
        echo "PASS $*"
    else
        echo "FAIL $*"
        fail=1
    fi
}

if [[ ! -f "$LOG" ]]; then
    echo "FAIL log not found: $LOG"
    exit 1
fi

PYTHONPATH=scratch /usr/bin/python3.14 -m v2x_analyzer "$LOG" --json > "$JSON"
rc=$?
check test "$rc" -eq 0

if [[ "$rc" -ne 0 ]]; then
    echo "P21_2_ANALYZER_ACCEPTANCE_EXIT=1"
    exit 1
fi

echo
echo "===== STATIC TAXONOMY CHECK ====="

if grep -Rni --include='*.py' \
    --exclude-dir='tests' \
    --exclude-dir='__pycache__' \
    -E 'PIPELINE_OK|NO_CERT|TX_SENT|BEACON' \
    scratch/v2x_analyzer >/tmp/p21_2_obsolete.txt
then
    echo "FAIL obsolete operational markers found"
    cat /tmp/p21_2_obsolete.txt
    fail=1
else
    echo "PASS no obsolete operational markers"
fi

echo
echo "===== FOCUSED PACKET-ID SEMANTICS ====="

PYTHONPATH=scratch /usr/bin/python3.14 - <<'PY'
from v2x_analyzer.parser import LogEntry
from v2x_analyzer.identity import build_identity_map
from v2x_analyzer.metrics.duplicates import compute

def mk(module, event, veh, node, station, seq, pkt, msg):
    return LogEntry(
        1, 1.0, "INFO", module, event, veh, node, station, seq,
        pkt, 0, None, None, None, None, None, None,
        msg, 1, "p21_2_acceptance",
    )

identity = build_identity_map([
    mk("VEHICLE", "IDENTITY_READY", "veh0", 0, 1, 0, None, ""),
    mk("VEHICLE", "IDENTITY_READY", "veh1", 1, 2, 0, None, ""),
])

entries = [
    mk(
        "CAM_RX", "RECEPTION_OK", "veh1", 1, 2, 0, 42,
        "sender=veh0 receiver=veh1 pkt=42 vpkt=0 seq=0",
    ),
    mk(
        "CAM_RX", "RECEPTION_OK", "veh1", 1, 2, 0, 42,
        "sender=veh0 receiver=veh1 pkt=42 vpkt=0 seq=0",
    ),
]

m = compute(entries, identity)

assert m.total_valid_receptions == 2
assert m.skipped_invalid_pkt == 0
assert m.duplicated_keys == 1
assert m.duplicate_count == 1
assert dict(m._counts) == {(1, 1, 42): 2}

print("PASS packetId used when seq=0")
print("PASS duplicate detection uses packetId")
PY
check test "$?" -eq 0

echo
echo "===== RAW LOG VS ANALYZER ====="

PYTHONPATH=scratch /usr/bin/python3.14 - "$LOG" "$JSON" <<'PY'
import json
import re
import sys
from collections import Counter

log_path, json_path = sys.argv[1:]

patterns = {
    "generate_ok": {
        ("CAM", "GENERATE_OK"),
        ("DENM", "GENERATE_OK"),
    },
    "encode_begin": {("CAM", "ENCODE_BEGIN")},
    "encode_ok": {
        ("CAM", "ENCODE_OK"),
        ("DENM", "ENCODE_OK"),
    },
    "sign_begin": {("SECURITY", "SIGN_BEGIN")},
    "sign_ok": {("SECURITY", "SIGN_OK")},
    "btp_tx_begin": {("BTP", "BTP_TX_BEGIN")},
    "btp_tx_ok": {("BTP", "BTP_TX_OK")},
    "geonet_tx_begin": {("GEONET", "GEONET_TX_BEGIN")},
    "geonet_tx_ok": {("GEONET", "GEONET_TX_OK")},
    "access_tx_begin": {("ACCESS", "ACCESS_TX_BEGIN")},
    "access_tx_ok": {("ACCESS", "ACCESS_TX_OK")},
    "send_begin": {("ACCESS", "SEND_BEGIN")},
    "send_ok": {("ACCESS", "SEND_OK")},
    "send_fail": {("ACCESS", "SEND_FAIL")},
    "tx_complete": {("TX", "TX_COMPLETE")},

    "raw_rx": {("RX", "RAW_RX")},
    "access_rx_begin": {("ACCESS", "ACCESS_RX_BEGIN")},
    "access_rx_ok": {("ACCESS", "ACCESS_RX_OK")},
    "geonet_rx_begin": {("GEONET", "GEONET_RX_BEGIN")},
    "geonet_rx_ok": {("GEONET", "GEONET_RX_OK")},
    "btp_rx_begin": {("BTP", "BTP_RX_BEGIN")},
    "btp_rx_ok": {("BTP", "BTP_RX_OK")},
    "verify_begin": {("SECURITY", "VERIFY_BEGIN")},
    "verify_ok": {("SECURITY", "VERIFY_OK")},
    "verify_fail": {("SECURITY", "VERIFY_FAIL")},
    "decode_ok": {
        ("CAM", "DECODE_OK"),
        ("DENM", "DENM_DECODE_OK"),
    },
    "decode_fail": {("CAM", "DECODE_FAIL")},
    "replay_ok": {
        ("REPLAY", "REPLAY_OK"),
        ("DENM", "DENM_REPLAY_OK"),
    },
    "replay_drop": {
        ("REPLAY", "REPLAY_DROP"),
        ("DENM", "DENM_REPLAY_DROP"),
    },
    "reception_begin": {
        ("CAM_RX", "RECEPTION_BEGIN"),
    },
    "reception_ok": {
        ("CAM_RX", "RECEPTION_OK"),
        ("DENM", "DENM_RECEPTION_OK"),
    },
    "deliver_begin": {("RX", "DELIVER_BEGIN")},
    "deliver_ok": {
        ("RX", "DELIVER_OK"),
        ("DENM", "DENM_DELIVER_OK"),
    },
}

counts = Counter()

with open(log_path, errors="replace") as fh:
    for line in fh:
        m = re.search(r"\[MOD=([A-Za-z0-9_]+)\]\[EV=([A-Za-z0-9_]+)\]", line)
        if not m:
            continue
        event = (m.group(1), m.group(2))
        for metric, accepted in patterns.items():
            if event in accepted:
                counts[metric] += 1

data = json.load(open(json_path))

errors = 0
for section in ("tx", "rx"):
    for name in data[section]:
        if name not in patterns:
            continue
        actual = data[section][name]
        expected = counts[name]
        if actual != expected:
            print(f"FAIL {name}: analyzer={actual} raw={expected}")
            errors += 1

if errors:
    raise SystemExit(1)

print(f"PASS raw/analyzer TX+RX taxonomy counts agree ({len(patterns)} metrics)")
print(f"RAW_TX_COMPLETE={counts['tx_complete']}")
print(f"RAW_RECEPTION_OK={counts['reception_ok']}")
print(f"ANALYZER_TX_COMPLETE={data['tx']['tx_complete']}")
print(f"ANALYZER_RECEPTION_OK={data['rx']['reception_ok']}")
PY
check test "$?" -eq 0

echo
echo
echo "===== DUPLICATE METRICS ====="

/usr/bin/python3.14 - "$LOG" "$JSON" <<'PY2'
import json
import re
import sys
from collections import Counter

log_path, json_path = sys.argv[1:]

raw_keys = Counter()
raw_cam_reception = 0

with open(log_path, errors="replace") as fh:
    for line in fh:
        if "[MOD=CAM_RX][EV=RECEPTION_OK]" not in line:
            continue

        raw_cam_reception += 1

        sender = re.search(r"\bsender_station=(\d+)\b", line)
        receiver = re.search(r"\breceiver_node=(\d+)\b", line)
        packet = re.search(r"\bpkt=(\d+)\b", line)

        assert sender and receiver and packet, line

        key = (
            int(receiver.group(1)),
            int(sender.group(1)),
            int(packet.group(1)),
        )
        raw_keys[key] += 1

data = json.load(open(json_path))
dup = data["dup"]

raw_unique_packet_keys = len(raw_keys)
raw_duplicate_keys = sum(v > 1 for v in raw_keys.values())
raw_duplicate_count = sum(v - 1 for v in raw_keys.values() if v > 1)

print("RAW_CAM_RECEPTION_OK=", raw_cam_reception)
print("RAW_UNIQUE_PACKET_KEYS=", raw_unique_packet_keys)
print("RAW_DUPLICATED_KEYS=", raw_duplicate_keys)
print("RAW_DUPLICATE_COUNT=", raw_duplicate_count)
print("ANALYZER_TOTAL_VALID_RECEPTIONS=", dup["total_valid_receptions"])
print("ANALYZER_DUPLICATED_KEYS=", dup["duplicated_keys"])
print("ANALYZER_DUPLICATE_COUNT=", dup["duplicate_count"])
print("skipped_invalid_pkt=", dup["skipped_invalid_pkt"])
print("skipped_unresolved_sender=", dup["skipped_unresolved_sender"])

assert dup["total_valid_receptions"] == raw_cam_reception
assert dup["total_valid_receptions"] == raw_unique_packet_keys
assert dup["duplicated_keys"] == raw_duplicate_keys
assert dup["duplicate_count"] == raw_duplicate_count
assert dup["skipped_invalid_pkt"] == 0
assert dup["skipped_unresolved_sender"] == 0

print("PASS P21.2 real-log duplicate metrics agree with raw packet keys")
PY2
check test "$?" -eq 0
echo "===== EXIT CRITERIA ====="

/usr/bin/python3.14 - "$JSON" <<'PY'
import json
import sys

d = json.load(open(sys.argv[1]))

assert d["exit_criteria_all_pass"] is True

for item in d["exit_"]["criteria"]:
    print(item["name"], "=", "PASS" if item["passed"] else "FAIL")

print("PASS analyzer exit criteria")
PY
check test "$?" -eq 0

echo
echo "P21_2_ANALYZER_ACCEPTANCE_EXIT=$fail"
exit "$fail"
