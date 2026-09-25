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
    print("===== DENM BRIDGE VEHICLE PROCESSING TEST =====")

    bridge = load_bridge()

    detector = bridge.DenmHazardDetector()
    sock = FakeSocket()

    # Mock do estado SUMO:
    # - posição conhecida
    # - velocidade baixa
    # - travagem forte
    bridge.traci.vehicle.getPosition = lambda vid: (50.125, 70.250)
    bridge.traci.vehicle.getSpeed = lambda vid: 8.0
    bridge.traci.vehicle.getAngle = lambda vid: 90.0
    bridge.traci.vehicle.getAcceleration = lambda vid: -5.0

    ok = True

    ok &= expect(
        hasattr(bridge, "process_vehicle"),
        "bridge exposes process_vehicle()",
    )

    if not hasattr(bridge, "process_vehicle"):
        print("DENM_BRIDGE_VEHICLE_PROCESSING_EXIT=1")
        return 1

    result = bridge.process_vehicle(
        sock,
        detector,
        "veh0",
        2100000,
    )

    ok &= expect(
        result is True,
        "vehicle processing reports a newly emitted DENM",
    )

    ok &= expect(
        len(sock.sent) == 2,
        "one vehicle state and one DENM datagram are emitted",
    )

    vehicle_payload, vehicle_address = sock.sent[0]
    denm_payload, denm_address = sock.sent[1]

    if isinstance(vehicle_payload, bytes):
        vehicle_payload = vehicle_payload.decode()

    if isinstance(denm_payload, bytes):
        denm_payload = denm_payload.decode()

    ok &= expect(
        vehicle_payload.startswith("veh0,"),
        "first datagram remains the VEHICLE state",
    )

    ok &= expect(
        denm_payload.startswith("DENM_EVENT,"),
        "second datagram is DENM_EVENT",
    )

    ok &= expect(
        ",veh0," in denm_payload,
        "DENM identifies the same vehicle",
    )

    ok &= expect(
        "hard_brake" in denm_payload,
        "DENM preserves the detected hazard",
    )

    ok &= expect(
        "50.125" in denm_payload and "70.250" in denm_payload,
        "DENM preserves the SUMO position",
    )

    ok &= expect(
        "2100000" in denm_payload,
        "DENM preserves the simulation timestamp",
    )

    ok &= expect(
        vehicle_address == ("127.0.0.1", 7400),
        "VEHICLE uses the ns-3 UDP endpoint",
    )

    ok &= expect(
        denm_address == ("127.0.0.1", 7400),
        "DENM uses the same ns-3 UDP endpoint",
    )

    # O detector mantém o hazard ativo. O estado VEHICLE continua
    # a ser enviado, mas não deve gerar outro DENM.
    result_again = bridge.process_vehicle(
        sock,
        detector,
        "veh0",
        2200000,
    )

    ok &= expect(
        result_again is False,
        "same active hazard does not emit another DENM",
    )

    ok &= expect(
        len(sock.sent) == 3,
        "second vehicle-state cycle emits only VEHICLE",
    )

    print(
        "DENM_BRIDGE_VEHICLE_PROCESSING_EXIT="
        f"{0 if ok else 1}"
    )

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
