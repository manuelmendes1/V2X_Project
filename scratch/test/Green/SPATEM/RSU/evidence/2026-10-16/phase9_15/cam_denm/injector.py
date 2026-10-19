import socket
import time

PORT = 7400
DURATION = 11.0
RECEIVER_DELAY = 2.5
EVENT_DELAY = 3.1
TRIGGER_TIME_US = 3_000_000

vehicles = {
    "veh0": (100.0, 100.0),
    "veh1": (120.0, 100.0),
}
counts = {v: 0 for v in vehicles}
start = time.monotonic()
deadline = start + DURATION
event_sent = False

event = (
    "DENM_EVENT,veh0,phase9_15_cam_denm_001,DETECTED,"
    "100.0,100.0,94,1000.0,5000,"
    f"{TRIGGER_TIME_US},PHASE_9_15_CAM_DENM"
).encode()

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    while time.monotonic() < deadline:
        elapsed = time.monotonic() - start

        for vid, (x, y) in vehicles.items():
            if vid == "veh1" and elapsed < RECEIVER_DELAY:
                continue

            payload = (
                f"{vid},{x:.1f},{y:.1f},10.0,0.0,0.0"
            ).encode()
            sock.sendto(payload, ("127.0.0.1", PORT))
            counts[vid] += 1

        if not event_sent and elapsed >= EVENT_DELAY:
            sock.sendto(event, ("127.0.0.1", PORT))
            event_sent = True
            print(f"DENM_EVENT_SENT=1 elapsed_s={elapsed:.3f}", flush=True)

        time.sleep(0.1)

for vid, count in counts.items():
    print(f"VEHICLE_ID={vid} STATE_DATAGRAMS_SENT={count}")

print(f"DENM_EVENT_SENT_COUNT={int(event_sent)}")
print("INJECTOR=PASS" if event_sent else "INJECTOR=FAIL")
