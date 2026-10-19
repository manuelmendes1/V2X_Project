import socket
import time

duration = 12.0
receiver_delay = 2.5
start = time.monotonic()
deadline = start + duration
counts = {"veh0": 0, "veh1": 0}

vehicles = {
    "veh0": (100.0, 100.0),
    "veh1": (120.0, 100.0),
}

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    while time.monotonic() < deadline:
        elapsed = time.monotonic() - start

        for vid, (x, y) in vehicles.items():
            if vid == "veh1" and elapsed < receiver_delay:
                continue

            payload = (
                f"{vid},{x:.1f},{y:.1f},10.0,0.0,0.0"
            ).encode()
            sock.sendto(payload, ("127.0.0.1", 7400))
            counts[vid] += 1

        time.sleep(0.1)

for vid, n in counts.items():
    print(f"VEHICLE_ID={vid} STATE_DATAGRAMS_SENT={n}")

print("INJECTOR=PASS")
