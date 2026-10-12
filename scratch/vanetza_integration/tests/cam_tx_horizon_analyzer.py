#!/usr/bin/env python3

import re
import sys
from collections import defaultdict, deque

if len(sys.argv) != 3:
    print("usage: cam_tx_horizon_analyzer.py <log> <sim_duration>")
    sys.exit(2)

log = sys.argv[1]
sim_duration = sys.argv[2]

packet_re = re.compile(r"\[PKT=(\d+)\]")
event_re = re.compile(r"\[EV=([A-Z0-9_]+)\]")
timestamp_re = re.compile(r"\[TIMESTAMP=(\d+)us\]")
runtime_re = re.compile(
    r"\[DCC_RUNTIME\]\s+now_us=(\d+)\s+next_us=(\d+)"
)

states = defaultdict(lambda: {
    "events": set(),
    "request_us": None,
    "runtime_next_us": None,
    "cam": False,
})

request_queues = defaultdict(deque)
max_timestamp_us = 0

with open(log, encoding="utf-8", errors="replace") as f:
    for line in f:
        pm = packet_re.search(line)
        em = event_re.search(line)
        tm = timestamp_re.search(line)
        rm = runtime_re.search(line)

        pkt = int(pm.group(1)) if pm else None
        ev = em.group(1) if em else None
        timestamp_us = int(tm.group(1)) if tm else None

        if timestamp_us is not None:
            max_timestamp_us = max(max_timestamp_us, timestamp_us)

        if pkt is not None and ev is not None:
            st = states[pkt]
            st["events"].add(ev)

            if ev == "BTP_TX_OK" and "port=2001" in line:
                st["cam"] = True

            if ev == "DCC_REQUEST":
                st["request_us"] = timestamp_us
                if timestamp_us is not None:
                    request_queues[timestamp_us].append(pkt)

        if rm:
            now_us = int(rm.group(1))
            next_us = int(rm.group(2))

            queue = request_queues.get(now_us)
            if queue:
                request_pkt = queue.popleft()
                states[request_pkt]["runtime_next_us"] = next_us

horizon_us = int(round(float(sim_duration) * 1_000_000.0))

cam_packets = sorted(
    pkt for pkt, st in states.items()
    if st["cam"]
)

completed = []
pending = []
bad = []

for pkt in cam_packets:
    st = states[pkt]
    events = st["events"]

    if "DCC_DROP" in events:
        bad.append((pkt, "DCC_DROP"))
        continue

    if "TX_COMPLETE" in events:
        if "DCC_TX" not in events:
            bad.append((pkt, "TX_COMPLETE without DCC_TX"))
        else:
            completed.append(pkt)
        continue

    if "DCC_TX" in events:
        bad.append((pkt, "DCC_TX without TX_COMPLETE"))
        continue

    if events.intersection({
        "ACCESS_TX_BEGIN",
        "ACCESS_TX_OK",
        "SEND_BEGIN",
        "SEND_OK",
    }):
        bad.append(
            (pkt, "partial access-layer TX without TX_COMPLETE")
        )
        continue

    next_us = st["runtime_next_us"]

    if (
        "DCC_REQUEST" in events
        and next_us is not None
        and next_us >= horizon_us
    ):
        pending.append(pkt)
        continue

    bad.append(
        (
            pkt,
            "no TX terminal event and no DCC wakeup at/after horizon",
        )
    )

print(
    "[INFO] CAM TX horizon=%dus" % horizon_us
)

print(
    "[INFO] CAM TX packets=%d completed=%d pending_at_horizon=%d"
    % (len(cam_packets), len(completed), len(pending))
)

if pending:
    print(
        "[PASS] PENDING_AT_HORIZON packets=%s"
        % ",".join(str(pkt) for pkt in pending)
    )

    for pkt in pending:
        st = states[pkt]
        print(
            "[INFO] PKT=%d request_us=%s next_dcc_us=%s"
            % (
                pkt,
                st["request_us"],
                st["runtime_next_us"],
            )
        )

if bad:
    print("[FAIL] CAM TX invalid terminal states:")
    for pkt, reason in bad:
        print("[FAIL] PKT=%d %s" % (pkt, reason))
    sys.exit(1)

if len(completed) + len(pending) != len(cam_packets):
    print("[FAIL] CAM TX terminal accounting mismatch")
    sys.exit(1)

for ev in (
    "ACCESS_TX_BEGIN",
    "ACCESS_TX_OK",
    "SEND_BEGIN",
    "SEND_OK",
    "TX_COMPLETE",
):
    actual = sum(
        1 for pkt in completed
        if ev in states[pkt]["events"]
    )

    if actual != len(completed):
        print(
            "[FAIL] %-20s expected=%d actual=%d"
            % (ev, len(completed), actual)
        )
        sys.exit(1)

dcc_tx_count = sum(
    1 for pkt in completed
    if "DCC_TX" in states[pkt]["events"]
)

if dcc_tx_count != len(completed):
    print(
        "[FAIL] DCC_TX expected=%d actual=%d"
        % (len(completed), dcc_tx_count)
    )
    sys.exit(1)

print(
    "[PASS] CAM TX terminal accounting: completed=%d pending_at_horizon=%d total=%d"
    % (len(completed), len(pending), len(cam_packets))
)

print(
    "[PASS] CAM TX completed chain: "
    "DCC_TX -> ACCESS_TX -> SEND -> TX_COMPLETE = %d"
    % len(completed)
)

sys.exit(0)
