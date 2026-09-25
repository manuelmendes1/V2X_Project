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
    print("===== DENM HAZARD DETECTOR RECOVERY TEST =====")

    bridge = load_bridge()

    detector = bridge.DenmHazardDetector()

    ok = True

    # ---------------------------------------------------------
    # 1. Primeiro hard brake -> DETECTED
    # ---------------------------------------------------------
    first = detector.evaluate(
        vehicle_id="veh0",
        x=10.0,
        y=20.0,
        speed=8.0,
        accel=-5.0,
        timestamp_us=2100000,
    )

    ok &= expect(
        first is not None,
        "initial hard-brake hazard is detected",
    )

    ok &= expect(
        first["event_type"] == "DETECTED",
        "initial event is DETECTED",
    )

    ok &= expect(
        first["event_id"] == "veh0-hard_brake-0001",
        "initial event gets sequence 0001",
    )

    ok &= expect(
        detector.active_event_count() == 1,
        "hard-brake event becomes active",
    )

    # ---------------------------------------------------------
    # 2. Veículo continua em situação normal -> recovery
    # ---------------------------------------------------------
    recovery = detector.evaluate(
        vehicle_id="veh0",
        x=11.0,
        y=21.0,
        speed=25.0,
        accel=0.0,
        timestamp_us=2200000,
    )

    ok &= expect(
        recovery is None,
        "recovery does not generate a DENM",
    )

    ok &= expect(
        detector.active_event_count() == 0,
        "recovery clears the active hazard",
    )

    # ---------------------------------------------------------
    # 3. Novo hard brake depois da recuperação
    # ---------------------------------------------------------
    second = detector.evaluate(
        vehicle_id="veh0",
        x=12.0,
        y=22.0,
        speed=7.0,
        accel=-5.5,
        timestamp_us=2300000,
    )

    ok &= expect(
        second is not None,
        "new hard-brake hazard is detected after recovery",
    )

    if second is not None:
        ok &= expect(
            second["event_type"] == "DETECTED",
            "second event is DETECTED",
        )

        ok &= expect(
            second["event_id"] == "veh0-hard_brake-0002",
            "second event gets sequence 0002",
        )

    ok &= expect(
        detector.active_event_count() == 1,
        "new hazard becomes active again",
    )

    print(
        "DENM_HAZARD_DETECTOR_RECOVERY_EXIT="
        f"{0 if ok else 1}"
    )

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
