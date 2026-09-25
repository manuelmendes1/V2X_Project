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
    print("===== DENM BRIDGE DETECTOR INTEGRATION TEST =====")

    bridge = load_bridge()

    detector = bridge.DenmHazardDetector()

    sock = FakeSocket()

    ok = True

    ok &= expect(
        hasattr(bridge, "emit_denm_event_for_vehicle"),
        "bridge exposes emit_denm_event_for_vehicle()",
    )

    if not hasattr(bridge, "emit_denm_event_for_vehicle"):
        print("DENM_BRIDGE_DETECTOR_INTEGRATION_EXIT=1")
        return 1

    # Evento realista: travagem forte.
    bridge.traci.vehicle.getPosition = lambda vid: (123.456, 78.900)
    bridge.traci.vehicle.getSpeed = lambda vid: 8.0
    bridge.traci.vehicle.getAcceleration = lambda vid: -5.0

    emitted = bridge.emit_denm_event_for_vehicle(
        sock,
        detector,
        "veh0",
        2100000,
    )

    ok &= expect(
        emitted is True,
        "hard-brake hazard produces a DENM event",
    )

    ok &= expect(
        len(sock.sent) == 1,
        "detected hazard causes exactly one UDP transmission",
    )

    payload, address = sock.sent[0]

    if isinstance(payload, bytes):
        payload = payload.decode()

    ok &= expect(
        payload.startswith("DENM_EVENT,"),
        "detected hazard is serialized as DENM_EVENT",
    )

    ok &= expect(
        ",veh0," in payload,
        "vehicle ID comes from detected vehicle",
    )

    ok &= expect(
        "hard_brake" in payload,
        "hazard reason is preserved",
    )

    ok &= expect(
        "123.456" in payload and "78.900" in payload,
        "hazard position comes from SUMO state",
    )

    ok &= expect(
        "2100000" in payload,
        "simulation timestamp reaches DENM payload",
    )

    # O mesmo hazard permanece ativo: não deve gerar DENM repetido
    # em cada ciclo de simulação.
    emitted_again = bridge.emit_denm_event_for_vehicle(
        sock,
        detector,
        "veh0",
        2200000,
    )

    ok &= expect(
        emitted_again is False,
        "same active hazard is not retransmitted",
    )

    ok &= expect(
        len(sock.sent) == 1,
        "active hazard does not create a second UDP transmission",
    )

    print(
        "DENM_BRIDGE_DETECTOR_INTEGRATION_EXIT="
        f"{0 if ok else 1}"
    )

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
