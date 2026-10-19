import argparse
import socket
import time

parser = argparse.ArgumentParser()
parser.add_argument("--duration", type=float, default=11.0)
parser.add_argument("--receiver-delay", type=float, default=2.5)
parser.add_argument("--udp-port", type=int, default=7400)
args = parser.parse_args()

destination = ("127.0.0.1", args.udp_port)
start = time.monotonic()
deadline = start + args.duration
receiver_start = start + args.receiver_delay

vehicles = {
    "cam_reg_tx": (100.0, 100.0),
    "cam_reg_rx": (120.0, 100.0),
}
counts = {vehicle_id: 0 for vehicle_id in vehicles}

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    while time.monotonic() < deadline:
        elapsed = time.monotonic() - start
        for vehicle_id, (x, y) in vehicles.items():
            if vehicle_id == "cam_reg_rx" and time.monotonic() < receiver_start:
                continue

            payload = (
                f"{vehicle_id},{x:.1f},{y:.1f},10.0,0.0,0.0"
            ).encode("utf-8")
            sock.sendto(payload, destination)
            counts[vehicle_id] += 1
        time.sleep(0.1)

for vehicle_id, count in counts.items():
    print(f"VEHICLE_ID={vehicle_id} STATE_DATAGRAMS_SENT={count}")

print(f"RECEIVER_DELAY_SECONDS={args.receiver_delay}")
print("VEHICLE_INJECTOR=PASS")
