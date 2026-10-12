#!/usr/bin/env bash
set -u
set -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"

LOG="test-results/etapa9_6_6_spatem_rx_acceptance.log"

mkdir -p "$(dirname "$LOG")"

echo "============================================================"
echo "ETAPA 9.6.6 — REAL SPATEM RX ACCEPTANCE"
echo "============================================================"

echo
echo "===== BUILD ====="
./ns3 build spatem_real_rx_acceptance_test
BUILD_RC=$?
echo "BUILD_RC=$BUILD_RC"

if [ "$BUILD_RC" -ne 0 ]; then
    echo "RESULTADO 9.6.6: BUILD FAILURE"
    exit "$BUILD_RC"
fi

echo
echo "===== REAL V2V RUN ====="
set +e
./ns3 run spatem_real_rx_acceptance_test 2>&1 | tee "$LOG"
RUN_RC=${PIPESTATUS[0]}
set -e

echo
echo "RUN_RC=$RUN_RC"

echo
echo "===== SOCKET RX EVIDENCE ====="
grep -E \
    '\[SPATEM_RX_TEST\] socket_rx|SPATEM_SOCKET_RX_PACKETS=' \
    "$LOG" || true

echo
echo "===== RUNTIME RX EVIDENCE ====="
grep -E \
    '\[VRT_RX\] ReceivePacket called' \
    "$LOG" || true

echo
echo "===== SPATEM BTP DISPATCH EVIDENCE ====="
grep -E \
    '\[EV=SPATEM_RX_DISPATCH\].*port=2004.*payload=4.*aid=137' \
    "$LOG" || true

echo
echo "===== SPATEM DECODE REJECTION EVIDENCE ====="
grep -E \
    '\[STAGE=DECODE\].*\[RESULT=FAIL\].*\[EV=SPATEM_DECODE_FAIL\]' \
    "$LOG" || true

echo
echo "===== RECEPTION / DELIVERY EVIDENCE ====="
grep -E \
    '\[STAGE=RECEPTION\]|\[STAGE=DELIVER\]' \
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
        '\[EV=SPATEM_RX_DISPATCH\].*port=2004.*payload=4.*aid=137' \
        "$LOG" 2>/dev/null || true
)"

SPATEM_DECODE_FAIL_COUNT="$(
    grep -c \
        '\[STAGE=DECODE\].*\[RESULT=FAIL\].*\[EV=SPATEM_DECODE_FAIL\]' \
        "$LOG" 2>/dev/null || true
)"

SPATEM_RECEPTION_COUNT="$(
    grep -c \
        '\[STAGE=RECEPTION\].*\[EV=SPATEM_RECEPTION_OK\]' \
        "$LOG" 2>/dev/null || true
)"

SPATEM_DELIVER_COUNT="$(
    grep -c \
        '\[STAGE=DELIVER\].*\[EV=SPATEM_DELIVER_OK\]' \
        "$LOG" 2>/dev/null || true
)"

BTP_2004_UNHANDLED_COUNT="$(
    grep -c \
        '\[BTP_UNDISPATCHED\].*port=2004' \
        "$LOG" 2>/dev/null || true
)"

echo
echo "===== COUNTS ====="
echo "SOCKET_RX_COUNT=$SOCKET_RX_COUNT"
echo "VRT_RX_COUNT=$VRT_RX_COUNT"
echo "SPATEM_2004_DISPATCH_COUNT=$SPATEM_DISPATCH_COUNT"
echo "SPATEM_DECODE_FAIL_COUNT=$SPATEM_DECODE_FAIL_COUNT"
echo "SPATEM_RECEPTION_COUNT=$SPATEM_RECEPTION_COUNT"
echo "SPATEM_DELIVER_COUNT=$SPATEM_DELIVER_COUNT"
echo "BTP_2004_UNDISPATCHED_COUNT=$BTP_2004_UNHANDLED_COUNT"

echo
echo "===== GREEN DECISION ====="

if [ "$RUN_RC" -ne 0 ]; then
    echo "FAIL executable did not complete real V2V run"
    echo "RESULTADO 9.6.6: FAIL"
    exit 1
fi

if [ "$SOCKET_RX_COUNT" -lt 1 ]; then
    echo "FAIL no real veh1 socket RX observed"
    echo "RESULTADO 9.6.6: FAIL"
    exit 1
fi

if [ "$VRT_RX_COUNT" -lt 1 ]; then
    echo "FAIL veh1 Runtime::ReceivePacket() was not reached"
    echo "RESULTADO 9.6.6: FAIL"
    exit 1
fi

if [ "$SPATEM_DISPATCH_COUNT" -lt 1 ]; then
    echo "FAIL SPATEM BTP port 2004 was not dispatched"
    echo "RESULTADO 9.6.6: FAIL"
    exit 1
fi

if [ "$SPATEM_DECODE_FAIL_COUNT" -lt 1 ]; then
    echo "FAIL invalid 4-byte SPATEM payload did not fail at DECODE"
    echo "RESULTADO 9.6.6: FAIL"
    exit 1
fi

if [ "$SPATEM_RECEPTION_COUNT" -ne 0 ]; then
    echo "FAIL invalid SPATEM payload reached RECEPTION"
    echo "RESULTADO 9.6.6: FAIL"
    exit 1
fi

if [ "$SPATEM_DELIVER_COUNT" -ne 0 ]; then
    echo "FAIL invalid SPATEM payload reached DELIVER"
    echo "RESULTADO 9.6.6: FAIL"
    exit 1
fi

if [ "$BTP_2004_UNHANDLED_COUNT" -ne 0 ]; then
    echo "FAIL BTP port 2004 was still undispatched"
    echo "RESULTADO 9.6.6: FAIL"
    exit 1
fi

echo
echo "PASS real socket RX reached veh1"
echo "PASS veh1 Runtime::ReceivePacket() was reached"
echo "PASS BTP port 2004 dispatched to SPATEM runtime handler"
echo "PASS invalid 4-byte payload reached SPATEM DECODE"
echo "PASS invalid SPATEM payload was rejected at DECODE"
echo "PASS invalid payload did not reach RECEPTION"
echo "PASS invalid payload did not reach DELIVER"
echo "PASS BTP port 2004 was not undispatched"
echo
echo "RESULTADO 9.6.6 GREEN: PASS"
echo "SPATEM_REAL_RX=PASS"

exit 0
