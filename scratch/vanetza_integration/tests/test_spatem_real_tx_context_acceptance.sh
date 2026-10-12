#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"

LOG="test-results/etapa9/9.10/spatem_real_tx_context_acceptance.log"
mkdir -p "$(dirname "$LOG")"

echo "============================================================"
echo "ETAPA 9.10 — SPATEM REAL TX PACKETCONTEXT ACCEPTANCE"
echo "============================================================"

./ns3 run spatem_real_tx_context_test 2>&1 | tee "$LOG"
RUN_RC=${PIPESTATUS[0]}

echo
echo "===== TEST RC ====="
echo "RUN_RC=$RUN_RC"

FAIL=0

check_exact()
{
    local pattern="$1"
    local description="$2"

    if grep -Eq "$pattern" "$LOG"; then
        echo "PASS $description"
    else
        echo "FAIL $description"
        FAIL=1
    fi
}

# Test executable itself.
check_exact \
    'SPATEM_REAL_TX_CONTEXT_GREEN_EXIT=0' \
    'real PacketContext test completed GREEN'

check_exact \
    'SPATEM_REAL_TX_CONTEXT_SOCKET_PACKETS=[1-9][0-9]*' \
    'real ns-3 socket observed transmission'

# SPATEM semantic stages.
# Actual log order is:
# SIM_TIME -> TIMESTAMP -> PKT -> ... -> STAGE -> EV
check_exact \
    'SIM_TIME=0\.100000s.*TIMESTAMP=100000us.*PKT=9106.*STAGE=GENERATE.*EV=SPATEM_GENERATE_OK' \
    'SPATEM GENERATE uses correct PacketContext timing'

check_exact \
    'SIM_TIME=0\.100000s.*TIMESTAMP=100000us.*PKT=9106.*STAGE=ENCODE.*EV=SPATEM_ENCODE_OK' \
    'SPATEM ENCODE uses correct PacketContext timing'

# Transport stages for the same PacketContext.
check_exact \
    '\[PKT=9106\].*STAGE=BTP_TX.*EV=BTP_TX_OK.*port=2004.*aid=137' \
    'BTP TX uses port 2004 and AID 137'

check_exact \
    '\[PKT=9106\].*STAGE=GEONET_TX.*EV=GEONET_TX_OK.*transport=SHB.*profile=ITS_G5.*aid=OTHER' \
    'GeoNetworking TX completed with SHB / ITS_G5'

check_exact \
    '\[PKT=9106\].*STAGE=GEONET_TX.*EV=DCC_TX.*tx_sim_us=200000.*dcc_delay_us=100000' \
    'DCC delay is observable without changing PacketContext TX time'

check_exact \
    '\[PKT=9106\].*STAGE=ACCESS_TX.*EV=ACCESS_TX_OK.*bytes=156' \
    'Access TX completed'

check_exact \
    '\[PKT=9106\].*STAGE=SEND.*EV=SEND_OK.*bytes=156' \
    'ns-3 SEND completed'

check_exact \
    '\[PKT=9106\].*STAGE=TX_COMPLETE.*RESULT=OK.*EV=TX_COMPLETE' \
    'PacketContext reaches successful TX_COMPLETE'

echo
echo "===== TX CONTEXT EVENTS ====="

grep -E \
    '\[PKT=9106\].*EV=(SPATEM_GENERATE_OK|SPATEM_ENCODE_OK|BTP_TX_OK|GEONET_TX_OK|DCC_TX|ACCESS_TX_OK|SEND_OK|TX_COMPLETE)' \
    "$LOG" \
    || true

echo
echo "===== TX TIMING ====="

grep -E \
    'SIM_TIME=0\.100000s.*TIMESTAMP=100000us.*PKT=9106.*STAGE=(GENERATE|ENCODE|BTP_TX|GEONET_TX|ACCESS_TX|SEND|TX_COMPLETE)' \
    "$LOG" \
    || true

echo
echo "===== DIFF CHECK ====="

if git diff --check; then
    echo "PASS git diff --check"
else
    echo "FAIL git diff --check"
    FAIL=1
fi

echo
echo "===== FINAL STATUS ====="

if [ "$RUN_RC" -eq 0 ] && [ "$FAIL" -eq 0 ]; then
    echo "RESULTADO 9.10 ACCEPTANCE: PASS"
    exit 0
fi

echo "RESULTADO 9.10 ACCEPTANCE: FAIL"
exit 1
