#!/usr/bin/env python3

import ast
import collections
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

STRACE_FILE = ROOT / "ns3-sendto.strace"
BRIDGE_FILE = ROOT / "bridge.log"

SENDTO_PATTERN = re.compile(
    r'sendto\(\d+,\s*("(?:\\.|[^"\\])*"),\s*\d+,'
    r'.*?sin_port=htons\(7000\).*?\)\s+=\s+(\d+)'
)

PREFIX = "NS3 -> SUMO | "


def extract_sent_payloads():
    payloads = []

    for line in STRACE_FILE.read_text(errors="replace").splitlines():
        match = SENDTO_PATTERN.search(line)

        if not match:
            continue

        try:
            payload = ast.literal_eval(match.group(1))
            result = int(match.group(2))
        except (ValueError, SyntaxError):
            continue

        if result >= 0 and isinstance(payload, str):
            payloads.append(payload)

    return payloads


def main():
    bridge_lines = BRIDGE_FILE.read_text(errors="replace").splitlines()

    sent = extract_sent_payloads()

    received = [
        line.split(PREFIX, 1)[1]
        for line in bridge_lines
        if PREFIX in line
    ]

    sent_counter = collections.Counter(sent)
    received_counter = collections.Counter(received)

    matched = sent_counter & received_counter
    missing = sent_counter - received_counter
    unexpected = received_counter - sent_counter

    matched_count = sum(matched.values())
    missing_count = sum(missing.values())
    unexpected_count = sum(unexpected.values())

    accepted = sum(
        "NS3 vehicle state accepted |" in line
        for line in bridge_lines
    )

    rejected = sum(
        "NS3 vehicle state rejected |" in line
        for line in bridge_lines
    )

    invalid_utf8 = sum(
        "rejected datagram: invalid UTF-8" in line
        for line in bridge_lines
    )

    print(f"NS3_SENDTO_SUCCESS_COUNT={len(sent)}")
    print(f"BRIDGE_RECEIVE_LOG_COUNT={len(received)}")
    print(f"PAYLOAD_OCCURRENCES_MATCHED={matched_count}")
    print(f"PAYLOAD_OCCURRENCES_MISSING={missing_count}")
    print(f"UNEXPECTED_RECEIVE_OCCURRENCES={unexpected_count}")
    print(f"DISPATCHER_ACCEPTED_COUNT={accepted}")
    print(f"DISPATCHER_REJECTED_COUNT={rejected}")
    print(f"UTF8_REJECTED_COUNT={invalid_utf8}")

    passed = (
        len(sent) == 39
        and len(received) == 39
        and matched_count == 39
        and missing_count == 0
        and unexpected_count == 0
        and accepted == 39
        and rejected == 0
        and invalid_utf8 == 0
    )

    if passed:
        print("UDP_PAYLOAD_RECONCILIATION=PASS")
        print("NS3_TO_BRIDGE_DISPATCH_E2E=PASS")
        return 0

    print("UDP_PAYLOAD_RECONCILIATION=FAIL")
    print("NS3_TO_BRIDGE_DISPATCH_E2E=FAIL")

    if missing:
        print("MISSING_PAYLOADS:")
        for payload, count in missing.items():
            print(f"  count={count} payload={payload!r}")

    if unexpected:
        print("UNEXPECTED_PAYLOADS:")
        for payload, count in unexpected.items():
            print(f"  count={count} payload={payload!r}")

    return 1


if __name__ == "__main__":
    sys.exit(main())
