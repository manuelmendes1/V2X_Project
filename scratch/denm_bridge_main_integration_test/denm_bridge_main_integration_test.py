#!/usr/bin/env python3

import importlib.util
import sys
import types
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
        if isinstance(payload, bytes):
            payload = payload.decode()

        self.sent.append((payload, address))

    def close(self):
        pass


class FakeVehicle:
    def getIDList(self):
        return ["veh0"]

    def getPosition(self, vehicle_id):
        return (100.0, 200.0)

    def getSpeed(self, vehicle_id):
        return 8.0

    def getAngle(self, vehicle_id):
        return 90.0

    def getAcceleration(self, vehicle_id):
        return -5.0


class FakeSimulation:
    def __init__(self):
        self.steps = 0

    def getMinExpectedNumber(self):
        return 1 if self.steps < 2 else 0

    def getTime(self):
        return self.steps * 0.1


class FakeTraci(types.ModuleType):
    def __init__(self):
        super().__init__("traci")

        self.vehicle = FakeVehicle()
        self.simulation = FakeSimulation()
        self.started = False
        self.closed = False

    def start(self, args):
        self.started = True

    def simulationStep(self):
        self.simulation.steps += 1

    def close(self):
        self.closed = True


def load_bridge(fake_traci):
    sys.modules["traci"] = fake_traci

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
    print("===== DENM BRIDGE MAIN INTEGRATION TEST =====")

    fake_traci = FakeTraci()
    bridge = load_bridge(fake_traci)

    sock = FakeSocket()

    # Substitui o socket real usado pelo main().
    bridge.socket.socket = lambda *args, **kwargs: sock

    # Evita depender da receção UDP durante este teste.
    bridge.receive_from_ns3 = lambda socket: None

    ok = True

    bridge.main()

    ok &= expect(
        fake_traci.started,
        "SUMO simulation is started",
    )

    ok &= expect(
        fake_traci.closed,
        "SUMO simulation is closed",
    )

    vehicle_messages = [
        payload
        for payload, _ in sock.sent
        if not payload.startswith("DENM_EVENT,")
    ]

    denm_messages = [
        payload
        for payload, _ in sock.sent
        if payload.startswith("DENM_EVENT,")
    ]

    ok &= expect(
        len(vehicle_messages) == 2,
        "two SUMO vehicle states are emitted",
    )

    ok &= expect(
        len(denm_messages) == 1,
        "same active hazard produces exactly one DENM",
    )

    if denm_messages:
        denm = denm_messages[0]

        ok &= expect(
            ",veh0," in denm,
            "DENM identifies the SUMO vehicle",
        )

        ok &= expect(
            "hard_brake" in denm,
            "DENM contains the detected hazard",
        )

        ok &= expect(
            "100.000" in denm and "200.000" in denm,
            "DENM contains the SUMO position",
        )

        ok &= expect(
            ",DETECTED," in denm,
            "DENM is emitted as DETECTED",
        )

        # O timestamp não pode ser zero porque o C++ DenmTrigger
        # rejeita triggerTimestampUs == 0.
        fields = denm.split(",")

        ok &= expect(
            len(fields) == 11,
            "DENM payload contains exactly 11 fields",
        )

        if len(fields) == 11:
            ok &= expect(
                int(fields[9]) == 1,
                "first DENM timestamp preserves simulation origin with 1 us floor",
            )

    print(
        "DENM_BRIDGE_MAIN_INTEGRATION_EXIT="
        f"{0 if ok else 1}"
    )

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
