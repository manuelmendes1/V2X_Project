#!/usr/bin/env bash
set -u

LOG="${1:-output_log_etapa_8_12.txt}"

if [ ! -f "$LOG" ]; then
    echo "RESULTADO 8.12: FAIL"
    echo "ACCEPTANCE_EXIT=1"
    echo "FAIL: log not found: $LOG"
    exit 1
fi

python3 - "$LOG" <<'PY'
import re
import sys
from pathlib import Path

log_path = Path(sys.argv[1])
lines = log_path.read_text(errors="replace").splitlines()

def fields(line):
    return dict(re.findall(r'\[([A-Z0-9_]+)=([^\]]*)\]', line))

def packet_id(line):
    m = re.search(r'\[PKT=([0-9]+)\]', line)
    return int(m.group(1)) if m else None

def has_event(pkts, event, extra=None):
    for line in lines:
        if packet_id(line) not in pkts:
            continue
        if f"[EV={event}]" not in line:
            continue
        if extra and not all(x in line for x in extra):
            continue
        return True
    return False

fails = 0

def check(condition, message):
    global fails
    if condition:
        print(f"[PASS] {message}")
    else:
        print(f"[FAIL] {message}")
        fails += 1

print("===== ETAPA 8.12 — DENM E2E ACCEPTANCE =====")
print(f"LOG={log_path}")

# ------------------------------------------------------------------
# Global error criteria
# ------------------------------------------------------------------
validate_fail = sum("[EV=DENM_VALIDATE_FAIL]" in x for x in lines)
replay_fail = sum("[EV=DENM_REPLAY_FAIL]" in x for x in lines)

check(validate_fail == 0,
      f"DENM validation failures == 0 ({validate_fail})")

check(replay_fail == 0,
      f"DENM replay failures == 0 ({replay_fail})")

# ------------------------------------------------------------------
# Find DENM TX candidates and select one with a corresponding valid
# DENM delivery. A self-originated GBC is expected to be dropped and
# must not be selected as the E2E acceptance candidate.
#
# Current implementation exposes the DENM application boundary as
# GBC_REQUEST rather than DENM_TRIGGER/DENM_ENCODE_OK.
# ------------------------------------------------------------------
tx_candidates = []

for line in lines:
    if "[EV=GBC_REQUEST]" not in line:
        continue
    if "port=2002" not in line:
        continue
    if "transport=GBC" not in line:
        continue
    if "aid=37" not in line:
        continue

    f = fields(line)
    pkt = packet_id(line)
    station = f.get("STATION")
    action = re.search(r'action_seq=([0-9]+)', line)

    if pkt is not None and station is not None and action:
        tx_candidates.append(
            (pkt, station, int(action.group(1)), line)
        )

check(bool(tx_candidates),
      "DENM TX candidate observed through GBC_REQUEST port=2002 aid=37")

if not tx_candidates:
    print("RESULTADO 8.12: FAIL")
    print("ACCEPTANCE_EXIT=1")
    sys.exit(1)

# Build RX candidates from DENM_DECODE_OK, where the semantic
# origin and ActionID are explicitly available. DENM_DELIVER_OK does
# not repeat origin_station in the current log schema; it is linked
# to the semantic event through the same RX PKT.
rx_candidates = []

for line in lines:
    if "[EV=DENM_DECODE_OK]" not in line:
        continue

    origin = re.search(r'origin_station=([0-9]+)', line)
    action = re.search(r'action_seq=([0-9]+)', line)
    pkt = packet_id(line)

    if not origin or not action or pkt is None:
        continue

    rx_candidates.append(
        (pkt, origin.group(1), int(action.group(1)), line)
    )

# Select a TX candidate for which the same semantic DENM event reaches
# a valid receiver and generates DENM_DELIVER_OK on the corresponding
# RX PacketContext.
selected = None
selected_rx = None

for tx_candidate in tx_candidates:
    tx_pkt, tx_origin, tx_action, tx_line = tx_candidate

    for rx_pkt, rx_origin, rx_action, rx_line in rx_candidates:
        if rx_origin != tx_origin or rx_action != tx_action:
            continue

        rx_deliver = any(
            packet_id(line) == rx_pkt
            and "[EV=DENM_DELIVER_OK]" in line
            for line in lines
        )

        if not rx_deliver:
            continue

        # Resolve receiver from the GN source correlation emitted on
        # the same RX PacketContext.
        receiver = None

        for line in lines:
            if packet_id(line) != rx_pkt:
                continue
            if "[EV=DENM_GN_SOURCE]" not in line:
                continue

            m = re.search(r'receiver_station=([0-9]+)', line)
            if m:
                receiver = m.group(1)
                break

        if receiver is None:
            continue

        if tx_origin == receiver:
            # This is a self-originated return and is not an A -> B
            # E2E delivery candidate.
            continue

        selected = tx_candidate
        selected_rx = (
            rx_pkt,
            rx_origin,
            rx_action,
            receiver,
            rx_line,
        )
        break

    if selected is not None:
        break

check(selected is not None,
      "DENM TX candidate has a corresponding successful A -> B delivery")

if selected is None:
    print("[INFO] TX candidates:")
    for pkt, station, action, _ in tx_candidates:
        print(
            f"[INFO]   PKT={pkt} origin_station={station} "
            f"action_seq={action}"
        )

    print("[INFO] RX decode candidates:")
    for pkt, origin, action, _ in rx_candidates:
        print(
            f"[INFO]   RX PKT={pkt} origin_station={origin} "
            f"action_seq={action}"
        )

    print("RESULTADO 8.12: FAIL")
    print("ACCEPTANCE_EXIT=1")
    sys.exit(1)

tx_pkt, origin_station, action_seq, tx_line = selected
rx_pkt, rx_origin, rx_action, receiver_station, rx_line = selected_rx

print(
    f"[INFO] TX candidate PKT={tx_pkt} "
    f"origin_station={origin_station} action_seq={action_seq}"
)

check(
    rx_origin == origin_station and rx_action == action_seq,
    "TX/RX semantic correlation preserves origin_station + action_seq",
)

check(
    origin_station != receiver_station,
    f"semantic origin differs from receiver "
    f"({origin_station} != {receiver_station})",
)

print(
    f"[INFO] RX candidate PKT={rx_pkt} "
    f"origin_station={rx_origin} "
    f"receiver_station={receiver_station} "
    f"action_seq={rx_action}"
)

rx_packets = {rx_pkt}

# ------------------------------------------------------------------
# RX chain
# ------------------------------------------------------------------
check(has_event(rx_packets, "DENM_RX_DISPATCH",
                ["port=2002", "aid=37"]),
      "RX DENM dispatch port=2002 aid=37")

check(has_event(rx_packets, "GEONET_RX_OK"),
      "RX GeoNetworking indication")

check(has_event(rx_packets, "BTP_RX_OK", ["port=2002"]),
      "RX BTP OK port=2002")

check(has_event(rx_packets, "VERIFY_OK",
                ["DENM V3 cryptographic verification succeeded"]),
      "RX DENM cryptographic verification OK")

check(has_event(rx_packets, "DENM_GN_SOURCE",
                [f"origin_station={rx_origin}",
                 "immediate_sender="]),
      "RX GN source correlation observed")

check(has_event(rx_packets, "DENM_DECODE_OK",
                [f"origin_station={rx_origin}",
                 f"action_seq={rx_action}"]),
      "RX DENM decode OK")

check(has_event(rx_packets, "DENM_REPLAY_OK",
                [f"origin_station={rx_origin}",
                 f"action_seq={rx_action}"]),
      "RX DENM replay accepted")

check(has_event(rx_packets, "DENM_RECEPTION_OK",
                [f"origin_station={rx_origin}",
                 f"action_seq={rx_action}"]),
      "RX DENM reception OK")

check(has_event(rx_packets, "DENM_DELIVER_OK",
                [f"action_seq={rx_action}",
                 f"receiver_station={receiver_station}"]),
      "RX DENM delivery OK")

# ------------------------------------------------------------------
# Explicitly demonstrate that self-origin drops do not invalidate
# the E2E acceptance.
# ------------------------------------------------------------------
self_origin_drops = sum(
    "[EV=DENM_SELF_ORIGIN_DROP]" in x for x in lines
)

print(f"[INFO] DENM_SELF_ORIGIN_DROP={self_origin_drops}")

print()
if fails == 0:
    print("RESULTADO 8.12: PASS")
    print("ACCEPTANCE_EXIT=0")
    sys.exit(0)

print("RESULTADO 8.12: FAIL")
print("ACCEPTANCE_EXIT=1")
sys.exit(1)
PY
