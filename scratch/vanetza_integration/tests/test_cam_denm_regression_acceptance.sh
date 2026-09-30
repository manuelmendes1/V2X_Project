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

python3 - "$LOG" "$RUN_EXIT" <<'PY'
import re
import sys
from pathlib import Path

log_path = Path(sys.argv[1])
run_exit = int(sys.argv[2])

lines = log_path.read_text(errors="replace").splitlines()

EVENT_RE = re.compile(r"\[EV=([A-Z0-9_]+)\]")
PKT_RE = re.compile(r"\[PKT=([0-9]+)\]")


def event_lines(event):
    return [
        line for line in lines
        if f"[EV={event}]" in line
    ]


def count(event, predicate=None):
    matched = event_lines(event)
    if predicate is not None:
        matched = [line for line in matched if predicate(line)]
    return len(matched)


def packet_ids(event):
    result = set()

    for line in event_lines(event):
        match = PKT_RE.search(line)
        if match:
            result.add(int(match.group(1)))

    return result


def vehicle_ids():
    values = set()

    for line in lines:
        values.update(
            x for x in re.findall(r"\[VEH=([^\]]+)\]", line)
            if x != "-"
        )

    return values


fails = 0


def check(condition, message):
    global fails

    if condition:
        print(f"[PASS] {message}")
    else:
        print(f"[FAIL] {message}")
        fails += 1


print("===== P19 — FINAL CAM/DENM REGRESSION ACCEPTANCE =====")
print(f"LOG={log_path}")
print(f"RUN_EXIT={run_exit}")

print()
print("===== RUNTIME =====")
check(
    run_exit == 0,
    f"simulation exit code == 0 ({run_exit})",
)

print()
print("===== CAM TX =====")

cam_btp_tx = count(
    "BTP_TX_OK",
    lambda x: "port=2001" in x
    and "transport=SHB" in x
    and "aid=36" in x,
)

cam_geonet_tx = count(
    "GEONET_TX_OK",
    lambda x: "transport=SHB" in x
    and "profile=ITS_G5" in x
    and "aid=CA" in x,
)

cam_encode = count(
    "ENCODE_OK",
    lambda x: "[MOD=CAM]" in x,
)

cam_tx_complete = count(
    "TX_COMPLETE",
)

check(cam_encode > 0, f"CAM ENCODE_OK observed ({cam_encode})")
check(
    cam_btp_tx > 0,
    f"CAM BTP TX port=2001 / SHB / aid=36 observed ({cam_btp_tx})",
)
check(
    cam_geonet_tx > 0,
    f"CAM GeoNetworking TX SHB / ITS_G5 / AID=CA observed ({cam_geonet_tx})",
)
check(cam_tx_complete > 0, f"TX_COMPLETE observed ({cam_tx_complete})")

print()
print("===== CAM RX =====")

cam_btp_rx = count(
    "BTP_RX_OK",
    lambda x: "port=2001" in x,
)

cam_decode = count(
    "DECODE_OK",
    lambda x: "[MOD=CAM]" in x
    and "BTP dst_port=2001" in x,
)

cam_replay = count(
    "REPLAY_OK",
    lambda x: "BTP dst_port=2001" in x,
)

cam_reception = count(
    "RECEPTION_OK",
    lambda x: "[MOD=CAM_RX]" in x
    and "BTP dst_port=2001" in x,
)

cam_deliver = count(
    "DELIVER_OK",
    lambda x: "[MOD=RX]" in x
    and "BTP dst_port=2001" in x,
)

check(
    cam_btp_rx > 0,
    f"CAM BTP RX port=2001 observed ({cam_btp_rx})",
)
check(cam_decode > 0, f"CAM DECODE_OK observed ({cam_decode})")
check(cam_replay > 0, f"CAM REPLAY_OK observed ({cam_replay})")
check(
    cam_reception > 0,
    f"CAM RECEPTION_OK observed ({cam_reception})",
)
check(
    cam_deliver > 0,
    f"CAM DELIVER_OK observed ({cam_deliver})",
)

print()
print("===== DENM TX =====")

denm_request = count(
    "GBC_REQUEST",
    lambda x: "port=2002" in x
    and "aid=37" in x
    and "transport=GBC" in x,
)

denm_request_ok = count(
    "GBC_REQUEST_OK",
    lambda x: "port=2002" in x
    and "aid=37" in x,
)

check(
    denm_request > 0,
    f"DENM GBC_REQUEST port=2002 / aid=37 / GBC observed ({denm_request})",
)
check(
    denm_request_ok > 0,
    f"DENM GBC_REQUEST_OK observed ({denm_request_ok})",
)

print()
print("===== DENM RX =====")

denm_dispatch = count(
    "DENM_RX_DISPATCH",
    lambda x: "port=2002" in x
    and "aid=37" in x,
)

denm_gn_source = count("DENM_GN_SOURCE")
denm_decode = count("DENM_DECODE_OK")
denm_replay_ok = count("DENM_REPLAY_OK")
denm_reception = count("DENM_RECEPTION_OK")
denm_deliver = count("DENM_DELIVER_OK")

check(
    denm_dispatch > 0,
    f"DENM_RX_DISPATCH port=2002 / aid=37 observed ({denm_dispatch})",
)
check(
    denm_gn_source > 0,
    f"DENM_GN_SOURCE observed ({denm_gn_source})",
)
check(
    denm_decode > 0,
    f"DENM_DECODE_OK observed ({denm_decode})",
)
check(
    denm_replay_ok > 0,
    f"DENM_REPLAY_OK observed ({denm_replay_ok})",
)
check(
    denm_reception > 0,
    f"DENM_RECEPTION_OK observed ({denm_reception})",
)
check(
    denm_deliver > 0,
    f"DENM_DELIVER_OK observed ({denm_deliver})",
)

print()
print("===== COEXISTENCE =====")

check(
    cam_encode > 0 and denm_request > 0,
    "CAM and DENM are both active in the same runtime log",
)

vehicles = vehicle_ids()
check(
    len(vehicles) >= 2,
    f"at least two vehicles observed ({len(vehicles)}: "
    + ", ".join(sorted(vehicles))
    + ")",
)

print()
print("===== DENM SECURITY NEGATIVE CONTROLS =====")

denm_replay_drop = count("DENM_REPLAY_DROP")
denm_self_origin = count("DENM_SELF_ORIGIN_DROP")

print(f"[INFO] DENM_REPLAY_DROP={denm_replay_drop}")
print(f"[INFO] DENM_SELF_ORIGIN_DROP={denm_self_origin}")

deliver_packets = packet_ids("DENM_DELIVER_OK")
reception_packets = packet_ids("DENM_RECEPTION_OK")

replay_drop_packets = packet_ids("DENM_REPLAY_DROP")
self_origin_packets = packet_ids("DENM_SELF_ORIGIN_DROP")

replay_drop_delivered = replay_drop_packets & (
    deliver_packets | reception_packets
)

self_origin_delivered = self_origin_packets & (
    deliver_packets | reception_packets
)

check(
    not replay_drop_delivered,
    "DENM replay-dropped packets do not reach reception/delivery "
    f"({len(replay_drop_delivered)} violations)",
)

check(
    not self_origin_delivered,
    "DENM self-origin-dropped packets do not reach reception/delivery "
    f"({len(self_origin_delivered)} violations)",
)

print()
print("===== CRYPTO / DECODE FAILURES =====")

failure_events = [
    "ENCODE_FAIL",
    "SIGN_FAIL",
    "VERIFY_FAIL",
    "DECODE_FAIL",
    "SEND_FAIL",
    "DENM_VALIDATE_FAIL",
    "DENM_REPLAY_FAIL",
    "DENM_DECODE_FAIL",
]

failure_total = 0

for event in failure_events:
    event_count = count(event)
    failure_total += event_count
    print(f"[INFO] {event}={event_count}")

check(
    failure_total == 0,
    f"no operational/cryptographic/decode failures ({failure_total})",
)

print()
print("===== FINAL RESULT =====")

if fails == 0:
    print("RESULTADO P19: PASS")
    print("P19_ACCEPTANCE_EXIT=0")
    sys.exit(0)

print("RESULTADO P19: FAIL")
print(f"P19_ACCEPTANCE_EXIT=1")
sys.exit(1)
PY
