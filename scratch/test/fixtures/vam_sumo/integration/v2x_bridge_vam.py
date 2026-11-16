#!/usr/bin/env python3
"""Independent SUMO-to-ns-3 bridge for VAM EVALUATION_ONLY scenarios."""

import math
import os
from pathlib import Path
import socket
import time
from datetime import datetime

import traci


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SUMO_CFG = (
    PROJECT_ROOT / "scenario" / "vam" / "vam.sumocfg"
)

SUMO_CFG = os.environ.get("V2X_VAM_SUMO_CFG", str(DEFAULT_SUMO_CFG))
SUMO_BINARY = os.environ.get("V2X_SUMO_BINARY", "sumo")
STEP_LENGTH = float(os.environ.get("V2X_SUMO_STEP_LENGTH", "0.1"))
SIM_TIME = float(os.environ.get("V2X_VAM_SIM_TIME", "12.0"))

NS3_UDP_IP = os.environ.get("V2X_NS3_UDP_IP", "127.0.0.1")
NS3_UDP_PORT = int(os.environ.get("V2X_NS3_UDP_PORT", "7400"))

UINT64_MAX = (1 << 64) - 1


def log(message):
    now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{now}] {message}", flush=True)


def _csv_token(value, label):
    value = str(value)
    if not value or any(ch in value for ch in (",", "\n", "\r")):
        raise ValueError(f"{label} must be non-empty and CSV-safe")
    return value


def _timestamp_us():
    simulation_time = float(traci.simulation.getTime())

    if not math.isfinite(simulation_time) or simulation_time < 0:
        raise ValueError("SUMO simulation time must be finite and non-negative")

    timestamp_us = int(round(simulation_time * 1_000_000))

    if timestamp_us < 0 or timestamp_us > UINT64_MAX:
        raise ValueError("SUMO simulation timestamp is outside uint64 range")

    return timestamp_us


def _send(sock, message):
    sock.sendto(
        message.encode("utf-8"),
        (NS3_UDP_IP, NS3_UDP_PORT),
    )


def send_vehicle_to_ns3(sock, vehicle_id, timestamp_us):
    vehicle_id = _csv_token(vehicle_id, "vehicle ID")

    position = traci.vehicle.getPosition(vehicle_id)
    speed = float(traci.vehicle.getSpeed(vehicle_id))
    angle = float(traci.vehicle.getAngle(vehicle_id))
    acceleration = float(traci.vehicle.getAcceleration(vehicle_id))

    x = float(position[0])
    y = float(position[1])

    if not all(math.isfinite(v) for v in (x, y, speed, angle, acceleration)):
        raise ValueError(f"Non-finite vehicle observation: {vehicle_id}")

    if speed < 0:
        raise ValueError(f"Negative vehicle speed: {vehicle_id}")

    # Preserve the existing seven-field vehicle datagram contract.
    message = (
        f"{vehicle_id},{x:.3f},{y:.3f},"
        f"{speed:.3f},{angle:.3f},{acceleration:.3f},{timestamp_us}"
    )
    _send(sock, message)


def send_person_upsert(sock, person_id, timestamp_us):
    person_id = _csv_token(person_id, "person ID")
    position = traci.person.getPosition(person_id)

    x = float(position[0])
    y = float(position[1])
    speed = float(traci.person.getSpeed(person_id))
    angle = float(traci.person.getAngle(person_id))
    vehicle_class = _csv_token(
        traci.person.getVehicleClass(person_id),
        "SUMO person vehicle class",
    )

    if not all(math.isfinite(v) for v in (x, y, speed, angle)):
        raise ValueError(f"Non-finite person observation: {person_id}")

    if speed < 0:
        raise ValueError(f"Negative person speed: {person_id}")

    message = (
        f"SUMO_PERSON,UPSERT,{person_id},"
        f"{x:.3f},{y:.3f},{speed:.3f},{angle:.3f},"
        f"{timestamp_us},{vehicle_class}"
    )
    _send(sock, message)


def send_person_remove(sock, person_id, timestamp_us):
    person_id = _csv_token(person_id, "person ID")
    _send(sock, f"SUMO_PERSON,REMOVE,{person_id},{timestamp_us}")


def send_vehicle_remove(sock, vehicle_id, timestamp_us):
    vehicle_id = _csv_token(vehicle_id, "vehicle ID")
    _send(sock, f"SUMO_VEHICLE,REMOVE,{vehicle_id},{timestamp_us}")


def publish_current_step(sock, previous_vehicle_ids, previous_person_ids):
    """Publish one complete logical SUMO observation step.

    All current UPSERTs and all departure REMOVE events are emitted before
    SUMO_STEP_END. The step marker therefore closes the observation batch.
    """
    timestamp_us = _timestamp_us()

    current_vehicle_ids = set(traci.vehicle.getIDList())
    current_person_ids = set(traci.person.getIDList())

    for vehicle_id in sorted(current_vehicle_ids):
        send_vehicle_to_ns3(sock, vehicle_id, timestamp_us)

    for person_id in sorted(current_person_ids):
        send_person_upsert(sock, person_id, timestamp_us)

    for vehicle_id in sorted(set(previous_vehicle_ids) - current_vehicle_ids):
        send_vehicle_remove(sock, vehicle_id, timestamp_us)

    for person_id in sorted(set(previous_person_ids) - current_person_ids):
        send_person_remove(sock, person_id, timestamp_us)

    # This must remain the final datagram for this logical timepoint.
    _send(sock, f"SUMO_STEP_END,{timestamp_us}")

    log(
        f"[VAM_STEP_PUBLISHED] timestamp_us={timestamp_us} "
        f"vehicles={len(current_vehicle_ids)} "
        f"persons={len(current_person_ids)}"
    )

    return current_vehicle_ids, current_person_ids


def receive_from_ns3(sock):
    sock.setblocking(False)

    while True:
        try:
            data, _address = sock.recvfrom(2048)
        except BlockingIOError:
            return

        log(f"NS3 -> SUMO | {data.decode('utf-8', errors='replace')}")


def main():
    if not math.isfinite(STEP_LENGTH) or STEP_LENGTH <= 0:
        raise ValueError("V2X_SUMO_STEP_LENGTH must be finite and positive")

    if not math.isfinite(SIM_TIME) or SIM_TIME <= 0:
        raise ValueError("V2X_VAM_SIM_TIME must be finite and positive")

    if not Path(SUMO_CFG).is_file():
        raise FileNotFoundError(f"SUMO configuration not found: {SUMO_CFG}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    traci_started = False
    previous_vehicle_ids = set()
    previous_person_ids = set()

    try:
        log(f"[VAM_BRIDGE] starting SUMO config={SUMO_CFG}")

        traci.start(
            [
                SUMO_BINARY,
                "-c",
                SUMO_CFG,
                "--step-length",
                str(STEP_LENGTH),
            ]
        )
        traci_started = True

        while float(traci.simulation.getTime()) < SIM_TIME:
            step_started = time.monotonic()

            # Advance SUMO first. Every observation in the batch, including
            # SUMO_STEP_END, now refers to the same completed simulation time.
            traci.simulationStep()

            (
                previous_vehicle_ids,
                previous_person_ids,
            ) = publish_current_step(
                sock,
                previous_vehicle_ids,
                previous_person_ids,
            )

            receive_from_ns3(sock)

            remaining = STEP_LENGTH - (time.monotonic() - step_started)
            if remaining > 0:
                time.sleep(remaining)

    except KeyboardInterrupt:
        log("[VAM_BRIDGE] interrupted")

    finally:
        try:
            if traci_started:
                traci.close()
        finally:
            sock.close()
            log("[VAM_BRIDGE] closed")


if __name__ == "__main__":
    main()
