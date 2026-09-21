#!/usr/bin/env bash
set -u

ROOT="$HOME/final/ns-3.47"
LOG="$ROOT/output_log_denm_replay_protection_test.txt"

cd "$ROOT" || exit 1

unset V2X_DENM_TAMPER
export V2X_DENM_DUPLICATE=1
export V2X_DENM_PKI_DIR="$HOME/vanetza-pki-lab/validation-denm-pki"

./build/scratch_v2x_sumo \
  --enableDenm=1 \
  --denmRadius=1000 \
  --simDuration="${V2X_TEST_SIM_DURATION:-60}" \
  > "$LOG" 2>&1

run_exit=$?

if [ "$run_exit" -ne 0 ]; then
    echo "FAIL: simulator exit_code=$run_exit"
    exit 1
fi

python3 - "$LOG" <<'PY'
import re
import sys
from collections import defaultdict

log_path = sys.argv[1]

replay_ok = []
replay_drop = []
deliver = []
verify_success = 0
duplicate_count = 0

denm_rx_count = 0

with open(log_path, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        if "DENM_V3_DUPLICATE" in line and "aid=37" in line:
            duplicate_count += 1

        if "EV=ACCESS_RX_OK" in line and "bytes=339" in line:
            denm_rx_count += 1
        if "DENM_V3_VERIFY" in line and \
           "aid=37" in line and \
           "report=SUCCESS" in line and \
           "cryptographic=PASS" in line:
            verify_success += 1

        if "EV=DENM_REPLAY_OK" in line:
            receiver = re.search(r"\[VEH=([^\]]+)\]", line)
            origin = re.search(r"origin_station=(\d+)", line)
            seq = re.search(r"action_seq=(\d+)", line)
            pkt = re.search(r"\[PKT=(\d+)\]", line)

            if receiver and origin and seq and pkt:
                replay_ok.append((
                    receiver.group(1),
                    int(origin.group(1)),
                    int(seq.group(1)),
                    int(pkt.group(1)),
                ))

        if "EV=DENM_REPLAY_DROP" in line:
            receiver = re.search(r"\[VEH=([^\]]+)\]", line)
            origin = re.search(r"origin_station=(\d+)", line)
            seq = re.search(r"action_seq=(\d+)", line)
            pkt = re.search(r"\[PKT=(\d+)\]", line)

            if receiver and origin and seq and pkt:
                replay_drop.append((
                    receiver.group(1),
                    int(origin.group(1)),
                    int(seq.group(1)),
                    int(pkt.group(1)),
                ))

        if "EV=DENM_DELIVER_OK" in line:
            pkt = re.search(r"\[PKT=(\d+)\]", line)
            if pkt:
                deliver.append(int(pkt.group(1)))

print(f"duplicate_count={duplicate_count}")
print(f"denm_rx_count={denm_rx_count}")
print(f"verify_success_count={verify_success}")
print(f"replay_ok_count={len(replay_ok)}")
print(f"replay_drop_count={len(replay_drop)}")
print(f"deliver_count={len(deliver)}")

if duplicate_count < 1:
    print("FAIL: no deterministic DENM duplicate was transmitted")
    sys.exit(1)

if denm_rx_count < 1:
    print("FAIL: no DENM reached ACCESS_RX")
    sys.exit(1)

if verify_success < 1:
    print("FAIL: no valid DENM V3 verification observed")
    sys.exit(1)

if not replay_ok:
    print("FAIL: no DENM_REPLAY_OK observed")
    sys.exit(1)

if not replay_drop:
    print("FAIL: no DENM_REPLAY_DROP observed")
    sys.exit(1)

accepted_keys = {
    (receiver, origin, seq)
    for receiver, origin, seq, _ in replay_ok
}

dropped_keys = {
    (receiver, origin, seq)
    for receiver, origin, seq, _ in replay_drop
}

common_keys = accepted_keys & dropped_keys

if not common_keys:
    print(
        "FAIL: no receiver received the same "
        "ActionID first as OK and later as DROP"
    )
    sys.exit(1)

for receiver, origin, seq, pkt in replay_drop:
    if pkt in deliver:
        print(
            f"FAIL: replay-dropped PKT={pkt} reached DELIVER"
        )
        sys.exit(1)

print("common_replay_keys=")
for key in sorted(common_keys):
    print(
        f"  receiver={key[0]} "
        f"origin_station={key[1]} "
        f"action_seq={key[2]}"
    )

print("PASS: DENM replay was accepted once per receiver and "
      "subsequent identical ActionID/payload copies were rejected "
      "before delivery")
PY
