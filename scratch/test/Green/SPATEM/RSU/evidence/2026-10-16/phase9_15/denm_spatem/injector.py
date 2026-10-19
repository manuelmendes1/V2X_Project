import socket
import time

duration = 12.0
receiver_delay = 2.5
event_at = 3.1

start = time.monotonic()
deadline = start + duration
counts = {"veh0": 0, "veh1": 0}
event_sent = False

vehicles = {
    "veh0": (100.0, 100.0),
    "veh1": (120.0, 100.0),
}

event = (
    "DENM_EVENT,veh0,phase9_15_denm_spatem_001,"
    "DETECTED,100.0,100.0,94,1000.0,5000,3000000,"
    "PHASE_9_15_DENM_SPATEM"
).encode()

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    while time.monotonic() < deadline:
        elapsed = time.monotonic() - start

        for vid, (x, y) in vehicles.items():
            if vid == "veh1" and elapsed < receiver_delay:
                continue

            payload = f"{vid},{x:.1f},{y:.1f},10.0,0.0,0.0".encode()
            sock.sendto(payload, ("127.0.0.1", 7400))
            counts[vid] += 1

        if not event_sent and elapsed >= event_at:
            sock.sendto(event, ("127.0.0.1", 7400))
            event_sent = True
            print("DENM_EVENT_SENT=1", flush=True)

        time.sleep(0.1)

for vid, count in counts.items():
    print(f"VEHICLE_ID={vid} STATE_DATAGRAMS_SENT={count}")

print(f"DENM_EVENT_SENT_FINAL={int(event_sent)}")
print("INJECTOR=PASS" if event_sent else "INJECTOR=FAIL")
