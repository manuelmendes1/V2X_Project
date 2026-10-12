#!/usr/bin/env python3

import argparse
import socket
import time

import traci


def parse_args():
    parser = argparse.ArgumentParser(
        description="Publish real SUMO/TraCI traffic-light state as SUMO_TLS UDP records."
    )

    parser.add_argument(
        "--sumo-binary",
        default="/usr/bin/sumo",
    )
    parser.add_argument(
        "--sumo-config",
        required=True,
    )
    parser.add_argument(
        "--udp-ip",
        default="127.0.0.1",
    )
    parser.add_argument(
        "--udp-port",
        type=int,
        required=True,
    )
    parser.add_argument(
        "--step-length",
        type=float,
        default=0.1,
    )
    parser.add_argument(
        "--sim-steps",
        type=int,
        default=420,
    )
    parser.add_argument(
        "--sleep",
        type=float,
        default=0.01,
    )
    parser.add_argument(
        "--tls-id",
        default=None,
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
    )

    return parser.parse_args()


def publish_tls(sock, destination, tls_id, controlled_links):
    program_id = int(traci.trafficlight.getProgram(tls_id))
    phase_index = traci.trafficlight.getPhase(tls_id)
    phase_duration = traci.trafficlight.getPhaseDuration(tls_id)
    next_switch = traci.trafficlight.getNextSwitch(tls_id)
    ryg = traci.trafficlight.getRedYellowGreenState(tls_id)
    simulation_time = traci.simulation.getTime()

    if len(ryg) < len(controlled_links):
        raise RuntimeError(
            f"TLS {tls_id}: RYG length {len(ryg)} < "
            f"controlled-link count {len(controlled_links)}"
        )

    records = []

    for link_index, links in enumerate(controlled_links):
        if not links:
            raise RuntimeError(
                f"TLS {tls_id}: linkIndex {link_index} has no controlled links"
            )

        raw_state = ryg[link_index]

        message = (
            f"SUMO_TLS,"
            f"{tls_id},"
            f"{link_index},"
            f"{raw_state},"
            f"{phase_index},"
            f"{phase_duration:.6f},"
            f"{next_switch:.6f},"
            f"{program_id},"
            f"{simulation_time:.6f}"
        )

        sock.sendto(message.encode("utf-8"), destination)
        records.append(message)

    return records


def main():
    args = parse_args()

    if args.step_length <= 0:
        raise ValueError("--step-length must be > 0")

    if args.sim_steps <= 0:
        raise ValueError("--sim-steps must be > 0")

    if args.udp_port <= 0 or args.udp_port > 65535:
        raise ValueError("--udp-port must be in 1..65535")

    traci.start([
        args.sumo_binary,
        "-c",
        args.sumo_config,
        "--step-length",
        str(args.step_length),
        "--no-step-log",
        "true",
    ])

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    destination = (args.udp_ip, args.udp_port)

    total_records = 0
    phase_changes = []
    previous_phase = None
    selected_tls = args.tls_id

    try:
        tls_ids = traci.trafficlight.getIDList()

        if args.verbose:
            print(f"TLS_IDS={tls_ids}")

        if not tls_ids:
            raise RuntimeError("SUMO exposes no traffic lights")

        if selected_tls is None:
            selected_tls = tls_ids[0]

        if selected_tls not in tls_ids:
            raise RuntimeError(
                f"Requested TLS {selected_tls!r} not present in {tls_ids}"
            )

        controlled_links = traci.trafficlight.getControlledLinks(
            selected_tls
        )

        if args.verbose:
            print(
                f"TLS_ID={selected_tls} "
                f"CONTROLLED_LINK_COUNT={len(controlled_links)}"
            )

        if not controlled_links:
            raise RuntimeError(
                f"TLS {selected_tls} has no controlled links"
            )

        for _ in range(args.sim_steps):
            phase_index = traci.trafficlight.getPhase(selected_tls)

            if phase_index != previous_phase:
                phase_changes.append(
                    (
                        traci.simulation.getTime(),
                        phase_index,
                        traci.trafficlight.getRedYellowGreenState(
                            selected_tls
                        ),
                        traci.trafficlight.getPhaseDuration(
                            selected_tls
                        ),
                        traci.trafficlight.getNextSwitch(
                            selected_tls
                        ),
                    )
                )

                if args.verbose:
                    current = phase_changes[-1]
                    print(
                        f"PHASE_CHANGE time={current[0]:.1f} "
                        f"phase={current[1]} "
                        f"ryg={current[2]} "
                        f"duration={current[3]:.1f} "
                        f"nextSwitch={current[4]:.1f}"
                    )

                previous_phase = phase_index

            records = publish_tls(
                sock,
                destination,
                selected_tls,
                controlled_links,
            )

            total_records += len(records)

            if args.verbose and len(phase_changes) == 1:
                for record in records:
                    print(f"SUMO_TLS={record}")

            traci.simulationStep()
            time.sleep(args.sleep)

    finally:
        traci.close()
        sock.close()

    print(f"TLS_ID={selected_tls}")
    print(f"TLS_RECORDS_SENT={total_records}")
    print(f"PHASE_CHANGES={len(phase_changes)}")

    for item in phase_changes:
        print(
            f"OBSERVED_PHASE time={item[0]:.1f} "
            f"phase={item[1]} "
            f"ryg={item[2]} "
            f"duration={item[3]:.1f} "
            f"nextSwitch={item[4]:.1f}"
        )

    if total_records <= 0:
        raise RuntimeError("No SUMO_TLS records were published")

    if len(phase_changes) < 3:
        raise RuntimeError(
            "Expected at least three observed TLS phases"
        )

    print("TRACI_SUMO_TLS_PUBLISHER=PASS")


if __name__ == "__main__":
    main()
