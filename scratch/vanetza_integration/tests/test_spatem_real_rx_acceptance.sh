#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"

LOG="test-results/etapa9_6_6_spatem_rx_red.log"

mkdir -p "$(dirname "$LOG")"

echo "============================================================"
echo "ETAPA 9.6.6 — REAL SPATEM RX RED"
echo "============================================================"

echo
echo "===== BUILD ====="
./ns3 build spatem_real_rx_red_test
BUILD_RC=$?
echo "BUILD_RC=$BUILD_RC"

if [ "$BUILD_RC" -ne 0 ]; then
    echo "RESULTADO 9.6.6 RED: BUILD FAILURE"
    exit "$BUILD_RC"
fi

echo
echo "===== REAL V2V RUN ====="
set +e
./ns3 run spatem_real_rx_red_test 2>&1 | tee "$LOG"
RUN_RC=${PIPESTATUS[0]}
set -e

echo
echo "RUN_RC=$RUN_RC"

echo
echo "===== RX SOCKET EVIDENCE ====="
grep -E \
    '\[SPATEM_RX_TEST\] socket_rx|SPATEM_SOCKET_RX_PACKETS=' \
    "$LOG" || true

echo
echo "===== RUNTIME RX EVIDENCE ====="
grep -E \
    '\[VRT_RX\] ReceivePacket called' \
    "$LOG" || true

echo
echo "===== GEONET/BTP DISPATCH EVIDENCE ====="
grep -E \
    '\[SPATEM_RX\] BTP handler accepted|\[EV=SPATEM_RX_DISPATCH\]' \
    "$LOG" || true

echo
echo "===== BTP UNDISPATCHED EVIDENCE ====="
grep -E \
    '\[BTP_UNDISPATCHED\].*port=2004' \
    "$LOG" || true

SOCKET_RX_COUNT="$(
    grep -c \
        '\[SPATEM_RX_TEST\] socket_rx.*node=1' \
        "$LOG" 2>/dev/null || true
)"

VRT_RX_COUNT="$(
    grep -c \
        '\[VRT_RX\] ReceivePacket called.*receiver_veh=veh1' \
        "$LOG" 2>/dev/null || true
)"

SPATEM_DISPATCH_COUNT="$(
    grep -c \
        '\[SPATEM_RX\] BTP handler accepted.*receiver=veh1.*port=2004.*payload=4.*aid=137' \
        "$LOG" 2>/dev/null || true
)"

BTP_2004_UNHANDLED_COUNT="$(
    grep -c \
        '\[BTP_UNDISPATCHED\].*port=2004' \
        "$LOG" 2>/dev/null || true
)"

BTP_RX_STAGE_COUNT="$(
    grep -c \
        '\[STAGE=BTP_RX\].*\[EV=SPATEM_RX_DISPATCH\].*port=2004.*payload=4.*aid=137' \
        "$LOG" 2>/dev/null || true
)"

echo
echo "SOCKET_RX_COUNT=$SOCKET_RX_COUNT"
echo "VRT_RX_COUNT=$VRT_RX_COUNT"
echo "SPATEM_2004_DISPATCH_COUNT=$SPATEM_DISPATCH_COUNT"
echo "BTP_2004_UNDISPATCHED_COUNT=$BTP_2004_UNHANDLED_COUNT"
echo "SPATEM_BTP_RX_STAGE_COUNT=$BTP_RX_STAGE_COUNT"

echo
echo "===== GREEN DECISION ====="

if [ "$RUN_RC" -ne 0 ]; then
    echo "FAIL executable did not complete real V2V run"
    echo "RESULTADO 9.6.6 GREEN: FAIL"
    exit 1
fi

if [ "$SOCKET_RX_COUNT" -lt 1 ]; then
    echo "FAIL no real veh1 socket RX observed"
    echo "RESULTADO 9.6.6 GREEN: FAIL"
    exit 1
fi

if [ "$VRT_RX_COUNT" -lt 1 ]; then
    echo "FAIL veh1 Runtime::ReceivePacket() was not reached"
    echo "RESULTADO 9.6.6 GREEN: FAIL"
    exit 1
fi

if [ "$SPATEM_DISPATCH_COUNT" -lt 1 ]; then
    echo "FAIL SPATEM BTP 2004 handler did not receive veh1 payload"
    echo "RESULTADO 9.6.6 GREEN: FAIL"
    exit 1
fi

if [ "$BTP_RX_STAGE_COUNT" -lt 1 ]; then
    echo "FAIL PacketContext did not reach BTP_RX for SPATEM"
    echo "RESULTADO 9.6.6 GREEN: FAIL"
    exit 1
fi

if [ "$BTP_2004_UNHANDLED_COUNT" -ne 0 ]; then
    echo "FAIL BTP port 2004 was still undispatched"
    echo "RESULTADO 9.6.6 GREEN: FAIL"
    exit 1
fi

echo
echo "PASS real socket RX reached veh1"
echo "PASS veh1 Runtime::ReceivePacket() was reached"
echo "PASS BTP port 2004 dispatched to SPATEM runtime handler"
echo "PASS SPATEM payload size preserved: 4 bytes"
echo "PASS SPATEM ITS-AID preserved: 137"
echo "PASS PacketContext reached BTP_RX"
echo "PASS BTP port 2004 is no longer undispatched"
echo
echo "RESULTADO 9.6.6 GREEN: PASS"
echo "SPATEM_REAL_RX=PASS"
exit 0
fi

if [ "$SOCKET_RX_COUNT" -lt 1 ]; then
    echo "FAIL no real veh1 socket RX observed"
    echo "RESULTADO 9.6.6 RED: INVALID"
    exit 1
fi

if [ "$VRT_RX_COUNT" -lt 1 ]; then
    echo "FAIL veh1 Runtime::ReceivePacket() was not reached"
    echo "RESULTADO 9.6.6 RED: INVALID"
    exit 1
fi

if [ "$UNHANDLED_COUNT" -lt 1 ]; then
    echo "FAIL BTP port 2004 was not observed as undispatched"
    echo "RESULTADO 9.6.6 RED: NOT_REPRODUCED"
    exit 1
fi

echo
echo "PASS real socket RX reached veh1"
echo "PASS veh1 Runtime::ReceivePacket() was reached"
echo "PASS BTP dispatcher observed port=2004 without handler"
echo
echo "RESULTADO 9.6.6 RED: CONFIRMED"
echo "SPATEM_REAL_RX_RED=PASS"
echo "EXPECTED_TEST_FAILURE=YES"

# Deliberate non-zero exit: the required 2004 runtime binding is not present.
exit 1
