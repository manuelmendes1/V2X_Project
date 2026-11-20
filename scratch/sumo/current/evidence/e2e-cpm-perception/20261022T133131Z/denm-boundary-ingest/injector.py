import socket
import sys
import time
from pathlib import Path

log_path = Path(sys.argv[1])
sent_states = 0
sent_markers = 0
sent_events = 0

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    for step in range(1, 11):
        timestamp_us = step * 100_000

        states = (
            f"veh0,100.000,100.000,10.0,0.0,0.0,"
            f"{timestamp_us},-8.4340762,41.4252899",
            f"veh1,120.000,100.000,9.0,0.0,0.0,"
            f"{timestamp_us},-8.4340000,41.4254000",
        )

        for state in states:
            sock.sendto(state.encode("ascii"), ("127.0.0.1", 7400))
            sent_states += 1

        if step == 3:
            event = (
                "DENM_EVENT,veh0,denm_boundary_test_001,DETECTED,"
                "100.0,100.0,94,1000.0,5000,300000,"
                "CPM_STEP_BOUNDARY_REGRESSION"
            )
            sock.sendto(event.encode("ascii"), ("127.0.0.1", 7400))
            sent_events += 1

        marker = f"SUMO_STEP_END,{timestamp_us}"
        sock.sendto(marker.encode("ascii"), ("127.0.0.1", 7400))
        sent_markers += 1
        time.sleep(0.1)

with log_path.open("w") as output:
    output.write(f"VEHICLE_STATES_SENT={sent_states}\n")
    output.write(f"STEP_END_MARKERS_SENT={sent_markers}\n")
    output.write(f"DENM_EVENTS_SENT={sent_events}\n")

print(f"VEHICLE_STATES_SENT={sent_states}")
print(f"STEP_END_MARKERS_SENT={sent_markers}")
print(f"DENM_EVENTS_SENT={sent_events}")
print(
    "DENM_BOUNDARY_INJECTOR=PASS"
    if sent_states == 20 and sent_markers == 10 and sent_events == 1
    else "DENM_BOUNDARY_INJECTOR=FAIL"
)
