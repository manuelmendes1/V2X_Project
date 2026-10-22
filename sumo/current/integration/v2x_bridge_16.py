#!/usr/bin/env python3
import os
import math
import traci
import socket
import time
from datetime import datetime

# ===============================
# CONFIGURAÇÃO
# ===============================

SUMO_CFG = os.environ.get("V2X_SUMO_CFG", "../scenario/simple.sumocfg")
TLS_ID = os.environ.get("V2X_TLS_ID") or None
SUMO_BINARY = os.environ.get("V2X_SUMO_BINARY", "sumo")
STEP_LENGTH = float(os.environ.get("V2X_SUMO_STEP_LENGTH", "0.1"))
SIM_TIME = float(os.environ.get("V2X_SUMO_SIM_TIME", "30"))
NS3_UDP_IP = "127.0.0.1"
NS3_UDP_PORT = 7400
NS3_TIMEOUT = 0.1

# ===============================
# FUNÇÕES AUXILIARES
# ===============================
def log(msg):
    now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{now}] {msg}")


def serialize_tls_message(
    tls_id,
    link_index,
    raw_state,
    phase_index,
    phase_duration,
    next_switch,
    program_id,
    simulation_time,
):
    """Serialize one TLS observation using the C++ SUMO_TLS contract."""
    if not isinstance(tls_id, str) or not tls_id or "," in tls_id:
        raise ValueError("TLS ID must be non-empty and contain no comma")

    if isinstance(link_index, bool) or not isinstance(link_index, int):
        raise ValueError("TLS link index must be an integer")
    if not 0 <= link_index <= 255:
        raise ValueError("TLS link index must be in 0..255")

    if not isinstance(raw_state, str) or len(raw_state) != 1:
        raise ValueError("TLS raw state must be one character")
    if raw_state.isspace() or "," in raw_state:
        raise ValueError("TLS raw state must not be whitespace or a comma")

    if phase_index < 0 or program_id < 0:
        raise ValueError("TLS phase and program IDs must be non-negative")

    return (
        "SUMO_TLS,"
        f"{tls_id},"
        f"{link_index},"
        f"{raw_state},"
        f"{int(phase_index)},"
        f"{float(phase_duration):.6f},"
        f"{float(next_switch):.6f},"
        f"{int(program_id)},"
        f"{float(simulation_time):.6f}"
    )



def should_continue_simulation(
    min_expected_number,
    tls_enabled,
    sim_time,
    sim_time_limit,
):
    """With TLS, enforce the time limit; otherwise preserve vehicle-driven execution."""
    if tls_enabled:
        return sim_time < sim_time_limit

    return min_expected_number > 0


def resolve_traffic_light_id(traffic_light_ids, configured_tls_id):
    """Resolve a configured TLS ID without guessing among multiple signals."""
    available = list(traffic_light_ids)

    if configured_tls_id is not None:
        if configured_tls_id not in available:
            raise ValueError(
                f"Requested TLS {configured_tls_id!r} not present in "
                f"{available}"
            )
        return configured_tls_id

    if not available:
        return None

    if len(available) == 1:
        return available[0]

    raise ValueError(
        "SUMO exposes multiple traffic lights; set V2X_TLS_ID explicitly. "
        f"Available IDs: {available}"
    )


def publish_traffic_light_updates(sock, tls_id, controlled_links):
    """Publish one SUMO_TLS UDP record per controlled-link index."""
    if not controlled_links:
        raise ValueError(f"TLS {tls_id!r} has no controlled links")

    program_id = int(traci.trafficlight.getProgram(tls_id))
    phase_index = int(traci.trafficlight.getPhase(tls_id))
    phase_duration = float(
        traci.trafficlight.getPhaseDuration(tls_id)
    )
    next_switch = float(traci.trafficlight.getNextSwitch(tls_id))
    ryg = traci.trafficlight.getRedYellowGreenState(tls_id)
    simulation_time = float(traci.simulation.getTime())

    if len(ryg) < len(controlled_links):
        raise ValueError(
            f"TLS {tls_id!r}: RYG length {len(ryg)} is shorter than "
            f"controlled-link count {len(controlled_links)}"
        )

    records_sent = 0

    for link_index, links in enumerate(controlled_links):
        if not links:
            raise ValueError(
                f"TLS {tls_id!r}: linkIndex {link_index} has no "
                "controlled links"
            )

        raw_state = ryg[link_index]

        message = serialize_tls_message(
            tls_id=tls_id,
            link_index=link_index,
            raw_state=raw_state,
            phase_index=phase_index,
            phase_duration=phase_duration,
            next_switch=next_switch,
            program_id=program_id,
            simulation_time=simulation_time,
        )

        sock.sendto(
            message.encode("utf-8"),
            (NS3_UDP_IP, NS3_UDP_PORT),
        )
        records_sent += 1

        log(
            f"[SUMO_TLS_PUBLISH] tls_id={tls_id}"
            f" link_index={link_index}"
            f" phase={phase_index}"
            f" state={raw_state}"
            f" sim_time={simulation_time:.3f}"
        )
        log(f"SUMO -> NS3 | {message}")

    return records_sent

_WGS84_CONVERSION_WARNING_EMITTED = False


def send_vehicle_to_ns3(sock, vid):
    global _WGS84_CONVERSION_WARNING_EMITTED

    # Todos os valores pertencem ao estado lógico SUMO atual.
    simulation_time_s = float(traci.simulation.getTime())
    if not math.isfinite(simulation_time_s) or simulation_time_s < 0:
        raise ValueError("SUMO simulation time must be finite and non-negative")

    simulation_time_us = int(round(simulation_time_s * 1_000_000))
    if simulation_time_us > (1 << 64) - 1:
        raise ValueError("SUMO simulation time exceeds uint64 microseconds")

    pos = traci.vehicle.getPosition(vid)
    speed = traci.vehicle.getSpeed(vid)
    angle = traci.vehicle.getAngle(vid)
    accel = traci.vehicle.getAcceleration(vid)

    # Keep projected network coordinates x/y unchanged.
    # TraCI converts them to WGS84 longitude/latitude when available.
    geodetic_suffix = ""
    try:
        converted = traci.simulation.convertGeo(pos[0], pos[1])
        if not isinstance(converted, (tuple, list)) or len(converted) != 2:
            raise ValueError("convertGeo did not return a longitude/latitude pair")

        longitude_deg = float(converted[0])
        latitude_deg = float(converted[1])

        if (
            not math.isfinite(longitude_deg)
            or not math.isfinite(latitude_deg)
            or not -180.0 <= longitude_deg <= 180.0
            or not -90.0 <= latitude_deg <= 90.0
        ):
            raise ValueError("convertGeo returned invalid WGS84 coordinates")

        geodetic_suffix = f",{longitude_deg:.7f},{latitude_deg:.7f}"

    except Exception as exc:
        # Preserve the seven-field contract if geographic conversion fails.
        # Log the fallback once to avoid flooding logs on every vehicle step.
        if not _WGS84_CONVERSION_WARNING_EMITTED:
            log(
                "[SUMO_WGS84] Conversion unavailable; "
                "sending seven-field sample without geographic coordinates "
                f"(reason={type(exc).__name__})"
            )
            _WGS84_CONVERSION_WARNING_EMITTED = True

    # Fields 1-6: legacy vehicle state.
    # Field 7: logical SUMO time in microseconds.
    # Fields 8-9, when present: WGS84 longitude, latitude.
    # SUMO time is not an absolute ITS timestamp or wall-clock time.
    msg = (
        f"{vid},{pos[0]:.3f},{pos[1]:.3f},"
        f"{speed:.3f},{angle:.3f},{accel:.3f},{simulation_time_us}"
        f"{geodetic_suffix}"
    )
    sock.sendto(msg.encode(), (NS3_UDP_IP, NS3_UDP_PORT))
    log(f"SUMO -> NS3 | {msg}")

def receive_from_ns3(sock):
    """Drena apenas os datagramas já disponíveis do NS3."""
    sock.setblocking(False)
    received = []

    while True:
        try:
            data, addr = sock.recvfrom(1024)
        except BlockingIOError:
            break

        received.append(data.decode())

    for msg in received:
        log(f"NS3 -> SUMO | {msg}")


def send_denm_event(sock, event):
    """
    Send one DENM scenario event to the ns-3 UDP endpoint.

    trigger_us is the logical SUMO simulation time.
    emit_monotonic_us is only for cross-process transport measurement.
    """
    message = serialize_denm_event(event)

    emit_monotonic_us = time.monotonic_ns() // 1000

    sock.sendto(
        message.encode(),
        (NS3_UDP_IP, NS3_UDP_PORT),
    )

    log(
        f"[DENM_EVENT_EMIT]"
        f" vehicle={event['vehicle_id']}"
        f" event={event['event_id']}"
        f" trigger_us={event['timestamp_us']}"
        f" emit_monotonic_us={emit_monotonic_us}"
    )

    log(f"SUMO -> NS3 | {message}")


def emit_denm_event_for_vehicle(
    sock,
    detector,
    vehicle_id,
    timestamp_us,
):
    """
    Evaluate the current SUMO state of one vehicle and emit a DENM
    only when the detector identifies a new hazard.
    """
    position = traci.vehicle.getPosition(vehicle_id)
    speed = traci.vehicle.getSpeed(vehicle_id)
    acceleration = traci.vehicle.getAcceleration(vehicle_id)

    event = detector.evaluate(
        vehicle_id=vehicle_id,
        x=position[0],
        y=position[1],
        speed=speed,
        accel=acceleration,
        timestamp_us=timestamp_us,
    )

    if event is None:
        return False

    send_denm_event(sock, event)
    return True


def process_vehicle(
    sock,
    detector,
    vehicle_id,
    timestamp_us,
):
    """
    Process one SUMO vehicle state.

    The normal VEHICLE datagram is always emitted for CAM.
    A DENM datagram is emitted only when the persistent detector
    identifies a new hazard for this vehicle.
    """
    send_vehicle_to_ns3(sock, vehicle_id)

    return emit_denm_event_for_vehicle(
        sock,
        detector,
        vehicle_id,
        timestamp_us,
    )


def serialize_denm_event(event):
    """
    Serializa um evento DENM do cenário para o protocolo UDP
    consumido pelo SumoAdapter.

    Formato:
    DENM_EVENT,vehicleId,eventId,eventType,x,y,causeCode,
    relevanceDistanceMeters,validityDurationMs,triggerTimeUs,reason
    """
    return (
        "DENM_EVENT,"
        f"{event['vehicle_id']},"
        f"{event['event_id']},"
        f"{event['event_type']},"
        f"{event['x']:.3f},"
        f"{event['y']:.3f},"
        f"{event['cause_code']},"
        f"{event['relevance_distance']:.1f},"
        f"{event['validity_ms']},"
        f"{event['timestamp_us']},"
        f"{event['reason']}"
    )


class DenmHazardDetector:
    """
    Detecta transições de hazard no lado SUMO.

    A unidade de identidade do estado é:
        (vehicle_id, hazard_type)

    Cada combinação gera no máximo um DETECTED enquanto permanecer ativa.
    O lifecycle UPDATED/CLEARED será integrado posteriormente ao C++.
    """

    HAZARD_STOPPED = "stopped"
    HAZARD_HARD_BRAKE = "hard_brake"
    HAZARD_TRAFFIC_JAM = "traffic_jam"

    def __init__(self, cause_codes=None):
        # Valores não zero apenas para permitir a passagem do trigger.
        # O mapeamento ETSI definitivo deverá ser configurado explicitamente.
        self.cause_codes = cause_codes or {
            self.HAZARD_STOPPED: 1,
            self.HAZARD_HARD_BRAKE: 2,
            self.HAZARD_TRAFFIC_JAM: 3,
        }

        self._active = set()
        self._sequence = {}

    def _classify(self, speed, accel):
        if speed < 1:
            return self.HAZARD_STOPPED

        if accel < -4:
            return self.HAZARD_HARD_BRAKE

        if speed < 15:
            return self.HAZARD_TRAFFIC_JAM

        return None

    def evaluate(
        self,
        vehicle_id,
        x,
        y,
        speed,
        accel,
        timestamp_us,
    ):
        hazard_type = self._classify(speed, accel)

        if hazard_type is None:
            self._active = {
                key
                for key in self._active
                if key[0] != vehicle_id
            }
            return None

        key = (vehicle_id, hazard_type)

        if key in self._active:
            return None

        sequence = self._sequence.get(key, 0) + 1
        self._sequence[key] = sequence
        self._active.add(key)

        event_id = (
            f"{vehicle_id}-{hazard_type}-{sequence:04d}"
        )

        return {
            "vehicle_id": vehicle_id,
            "event_id": event_id,
            "event_type": "DETECTED",
            "x": float(x),
            "y": float(y),
            "cause_code": int(
                self.cause_codes[hazard_type]
            ),
            "relevance_distance": 1000.0,
            "validity_ms": 5000,
            "timestamp_us": int(timestamp_us),
            "reason": hazard_type,
        }

    def active_event_count(self):
        return len(self._active)

    def clear(self):
        self._active.clear()
        self._sequence.clear()


def detect_events(vid):
    speed = traci.vehicle.getSpeed(vid)
    accel = traci.vehicle.getAcceleration(vid)
    if speed < 1:
        log(f"EVENT | {vid} stopped / accident detected")
    elif accel < -4:
        log(f"EVENT | {vid} hard brake detected")
    elif speed < 15:
        log(f"EVENT | {vid} traffic jam detected")

# ===============================
# MAIN
# ===============================
def main():
    log("Starting SUMO bridge...")

    traci.start(
        [
            SUMO_BINARY,
            "-c",
            SUMO_CFG,
            "--step-length",
            str(STEP_LENGTH),
            "--no-step-log",
            "true",
        ]
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sim_time = 0.0
    detector = DenmHazardDetector()
    selected_tls = None
    controlled_links = ()
    tls_records_sent = 0

    try:
        tls_ids = traci.trafficlight.getIDList()
        selected_tls = resolve_traffic_light_id(tls_ids, TLS_ID)

        if selected_tls is not None:
            controlled_links = traci.trafficlight.getControlledLinks(
                selected_tls
            )
            if not controlled_links:
                raise ValueError(
                    f"TLS {selected_tls!r} exposes no controlled links"
                )

            log(
                f"[SUMO_TLS] ACTIVE tls_id={selected_tls}"
                f" controlled_link_count={len(controlled_links)}"
            )
        else:
            log(
                "[SUMO_TLS] DISABLED reason="
                "configured SUMO scenario exposes no traffic lights"
            )

        while should_continue_simulation(
            min_expected_number=traci.simulation.getMinExpectedNumber(),
            tls_enabled=selected_tls is not None,
            sim_time=sim_time,
            sim_time_limit=SIM_TIME,
        ):
            step_started = time.monotonic()
            veh_ids = traci.vehicle.getIDList()

            timestamp_us = max(
                1,
                int(round(sim_time * 1_000_000)),
            )

            # Preserve the existing vehicle and DENM behavior.
            for vid in veh_ids:
                process_vehicle(
                    sock,
                    detector,
                    vid,
                    timestamp_us,
                )

            # Publish actual TraCI traffic-light state, if this scenario
            # contains the explicitly selected (or sole) TLS.
            if selected_tls is not None:
                tls_records_sent += publish_traffic_light_updates(
                    sock,
                    selected_tls,
                    controlled_links,
                )

            # Delimit the complete batch for this logical SUMO step.
            step_end_message = f"SUMO_STEP_END,{timestamp_us}"
            sock.sendto(
                step_end_message.encode("ascii"),
                (NS3_UDP_IP, NS3_UDP_PORT),
            )
            log(f"SUMO -> NS3 | {step_end_message}")

            receive_from_ns3(sock)
            traci.simulationStep()
            sim_time += STEP_LENGTH

            # Pace SUMO against wall-clock time for ns-3 realtime.
            remaining = STEP_LENGTH - (time.monotonic() - step_started)
            if remaining > 0:
                time.sleep(remaining)

    except KeyboardInterrupt:
        log("Simulation interrupted by user")

    finally:
        try:
            traci.close()
        finally:
            sock.close()
            if selected_tls is not None:
                log(f"[SUMO_TLS] TLS_RECORDS_SENT={tls_records_sent}")
            log("Bridge terminated cleanly")

if __name__ == "__main__":
    main()
