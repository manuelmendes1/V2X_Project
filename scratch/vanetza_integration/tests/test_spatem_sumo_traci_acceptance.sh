#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

TEST_BIN="$ROOT/build/scratch/ns3.47-sumo_adapter_spatem_traci_test-default"
PUBLISHER="$ROOT/scratch/vanetza_integration/sumo_adapter/sumo_traffic_light_publisher.py"

PUBLISHER_LOG="$ROOT/test-results/etapa9/9.13_traci_tls_publisher.log"
ADAPTER_LOG="$ROOT/test-results/etapa9/9.13_traci_adapter_acceptance.log"

mkdir -p "$ROOT/test-results/etapa9"

echo "============================================================"
echo "ETAPA 9.13 — SUMO/TraCI -> UDP -> SumoAdapter ACCEPTANCE"
echo "============================================================"

test -x "$TEST_BIN"
test -f "$PUBLISHER"

rm -f "$PUBLISHER_LOG" "$ADAPTER_LOG"

python3 "$PUBLISHER" \
  --sumo-binary /usr/bin/sumo \
  --sumo-config /tmp/etapa9_13_tls/simple.sumocfg \
  --udp-ip 127.0.0.1 \
  --udp-port 7424 \
  --sim-steps 420 \
  --sleep 0.01 \
  --verbose \
  > "$PUBLISHER_LOG" 2>&1 &

PUBLISHER_PID=$!

sleep 0.25

"$TEST_BIN" \
  > "$ADAPTER_LOG" 2>&1

TEST_RC=$?

wait "$PUBLISHER_PID"
PUBLISHER_RC=$?

echo
echo "===== PUBLISHER OUTPUT ====="
cat "$PUBLISHER_LOG"

echo
echo "===== ADAPTER OUTPUT ====="
cat "$ADAPTER_LOG"

echo
echo "===== RESULT ====="

grep -q \
  'TRACI_SUMO_TLS_PUBLISHER=PASS' \
  "$PUBLISHER_LOG"

PUBLISHER_MARKER_RC=$?

grep -q \
  'SUMO_ADAPTER_SPATEM_TRACI_ACCEPTANCE=PASS' \
  "$ADAPTER_LOG"

ADAPTER_MARKER_RC=$?

echo "TEST_RC=$TEST_RC"
echo "PUBLISHER_RC=$PUBLISHER_RC"
echo "PUBLISHER_MARKER_RC=$PUBLISHER_MARKER_RC"
echo "ADAPTER_MARKER_RC=$ADAPTER_MARKER_RC"

if [ "$TEST_RC" -eq 0 ] &&
   [ "$PUBLISHER_RC" -eq 0 ] &&
   [ "$PUBLISHER_MARKER_RC" -eq 0 ] &&
   [ "$ADAPTER_MARKER_RC" -eq 0 ]; then
    echo "SUMO_SPATEM_TRACI_ACCEPTANCE=PASS"
    exit 0
fi

echo "SUMO_SPATEM_TRACI_ACCEPTANCE=FAIL"
exit 1
