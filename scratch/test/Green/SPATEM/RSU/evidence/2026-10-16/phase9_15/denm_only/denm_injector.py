import socket
import time

PORT = 7400
DURATION = 11.0
EVENT_DELAY = 3.0
TRIGGER_TIME_US = 3_000_000

vehicles = {
    "veh0": (100.0, 100.0),
    "veh1": (120.0, 100.0),
}
counts = {vehicle_id: 0 for vehicle_id in vehicles}
event_sent = False
start = time.monotonic()
deadline = start + DURATION

event = (
    "DENM_EVENT,veh0,phase9_15_event_001,DETECTED,"
    "100.0,100.0,94,1000.0,5000,"
    f"{TRIGGER_TIME_US},PHASE_9_15_DENM_ONLY"
).encode("utf-8")

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    while time.monotonic() < deadline:
        elapsed = time.monotonic() - start

        for vehicle_id, (x, y) in vehicles.items():
            state = (
                f"{vehicle_id},{x:.1f},{y:.1f},10.0,0.0,0.0"
            ).encode("utf-8")
            sock.sendto(state, ("127.0.0.1", PORT))
            counts[vehicle_id] += 1

        if not event_sent and elapsed >= EVENT_DELAY:
            sock.sendto(event, ("127.0.0.1", PORT))
            event_sent = True
            print(
                "DENM_EVENT_SENT=1 "
                f"elapsed_s={elapsed:.3f} "
                f"trigger_us={TRIGGER_TIME_US}",
                flush=True,
            )

        time.sleep(0.1)

for vehicle_id, count in counts.items():
    print(f"VEHICLE_ID={vehicle_id} STATE_DATAGRAMS_SENT={count}")

print(f"DENM_EVENT_SENT_COUNT={int(event_sent)}")
print("DENM_INJECTOR=PASS" if event_sent else "DENM_INJECTOR=FAIL")
