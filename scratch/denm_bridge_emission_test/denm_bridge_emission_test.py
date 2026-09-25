#!/usr/bin/env python3

import importlib.util
from pathlib import Path


BRIDGE = (
    Path(__file__).resolve().parents[3]
    / "v2x_sumo"
    / "integration"
    / "v2x_bridge_15.py"
)


def expect(condition, message):
    if not condition:
        print(f"FAIL {message}")
        return False

    print(f"PASS {message}")
    return True


class FakeSocket:
    def __init__(self):
        self.sent = []

    def sendto(self, payload, address):
        self.sent.append((payload, address))


def load_bridge():
    spec = importlib.util.spec_from_file_location(
        "v2x_bridge_15",
        BRIDGE,
    )

    if spec is None or spec.loader is None:
        raise RuntimeError(f"Não foi possível carregar {BRIDGE}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    print("===== DENM BRIDGE EMISSION TEST =====")

    bridge = load_bridge()

    event = {
        "vehicle_id": "veh0",
        "event_id": "veh0-hard_brake-0001",
        "event_type": "DETECTED",
        "x": 10.0,
        "y": 20.0,
        "cause_code": 2,
        "relevance_distance": 1000.0,
        "validity_ms": 5000,
        "timestamp_us": 2100000,
        "reason": "hard_brake",
    }

    sock = FakeSocket()

    ok = True

    ok &= expect(
        hasattr(bridge, "send_denm_event"),
        "bridge exposes send_denm_event()",
    )

    if not hasattr(bridge, "send_denm_event"):
        print("DENM_BRIDGE_EMISSION_EXIT=1")
        return 1

    bridge.send_denm_event(sock, event)

    ok &= expect(
        len(sock.sent) == 1,
        "DENM event is sent exactly once",
    )

    payload, address = sock.sent[0]

    if isinstance(payload, bytes):
        payload = payload.decode()

    ok &= expect(
        payload.startswith("DENM_EVENT,"),
        "UDP payload uses DENM_EVENT discriminator",
    )

    ok &= expect(
        "veh0" in payload,
        "vehicle ID is present in emitted payload",
    )

    ok &= expect(
        "veh0-hard_brake-0001" in payload,
        "event ID is present in emitted payload",
    )

    ok &= expect(
        "DETECTED" in payload,
        "event type is present in emitted payload",
    )

    ok &= expect(
        "2100000" in payload,
        "simulation timestamp is present in emitted payload",
    )

    ok &= expect(
        address == ("127.0.0.1", 7400),
        "DENM event uses the ns-3 UDP endpoint",
    )

    print(f"DENM_BRIDGE_EMISSION_EXIT={0 if ok else 1}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
