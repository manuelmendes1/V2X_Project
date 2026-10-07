#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"

LOG="test-results/etapa9/9.9/spatem_real_replay.log"

mkdir -p "$(dirname "$LOG")"

echo "============================================================"
echo "ETAPA 9.9 — SPATEM REAL REPLAY ACCEPTANCE"
echo "============================================================"

./ns3 run spatem_real_replay_test 2>&1 | tee "$LOG"
RUN_RC=${PIPESTATUS[0]}

echo
echo "RUN_RC=$RUN_RC"

FAIL=0

check_fixed() {
    local pattern="$1"
    local description="$2"

    if grep -qF "$pattern" "$LOG"; then
        echo "PASS $description"
    else
        echo "FAIL $description"
        FAIL=1
    fi
}

count_exact() {
    local pattern="$1"
    local expected="$2"
    local description="$3"

    local actual
    actual=$(grep -cF "$pattern" "$LOG" || true)

    if [[ "$actual" -eq "$expected" ]]; then
        echo "PASS $description ($actual)"
    else
        echo "FAIL $description expected=$expected actual=$actual"
        FAIL=1
    fi
}

check_fixed "PASS first real SPATEM TX accepted" \
    "first real SPATEM transmission accepted"

check_fixed "PASS second real SPATEM TX accepted by transport" \
    "second real SPATEM transmission accepted"

check_fixed "PASS real ns-3 socket observed both transmissions" \
    "real ns-3 socket observed both transmissions"

check_fixed "SPATEM_REAL_REPLAY_SOCKET_PACKETS=3" \
    "expected socket observation count"

count_exact "EV=SPATEM_RX_DISPATCH" 2 \
    "two SPATEM dispatches reached the receiver"

count_exact "EV=SPATEM_REPLAY_OK" 1 \
    "exactly one SPATEM accepted by replay protection"

count_exact "EV=SPATEM_REPLAY_DROP" 1 \
    "exactly one SPATEM rejected as replay"

count_exact "EV=SPATEM_RECEPTION_OK" 1 \
    "exactly one SPATEM reached reception"

count_exact "EV=SPATEM_DELIVER_OK" 1 \
    "exactly one SPATEM reached delivery"

OK_PKT=$(
    grep 'EV=SPATEM_REPLAY_OK' "$LOG" |
    sed -n 's/.*\[PKT=\([0-9][0-9]*\)\].*/\1/p' |
    tail -n 1
)

DROP_PKT=$(
    grep 'EV=SPATEM_REPLAY_DROP' "$LOG" |
    sed -n 's/.*\[PKT=\([0-9][0-9]*\)\].*/\1/p' |
    tail -n 1
)

echo "REPLAY_OK_PKT=$OK_PKT"
echo "REPLAY_DROP_PKT=$DROP_PKT"

if [[ -z "$OK_PKT" || -z "$DROP_PKT" ]]; then
    echo "FAIL could not extract replay packet identities"
    FAIL=1
else
    if grep -q "\[PKT=$OK_PKT\].*EV=SPATEM_RECEPTION_OK" "$LOG"; then
        echo "PASS accepted packet reached RECEPTION"
    else
        echo "FAIL accepted packet did not reach RECEPTION"
        FAIL=1
    fi

    if grep -q "\[PKT=$OK_PKT\].*EV=SPATEM_DELIVER_OK" "$LOG"; then
        echo "PASS accepted packet reached DELIVER"
    else
        echo "FAIL accepted packet did not reach DELIVER"
        FAIL=1
    fi

    if grep -q "\[PKT=$DROP_PKT\].*EV=SPATEM_RECEPTION_OK" "$LOG"; then
        echo "FAIL replayed packet reached RECEPTION"
        FAIL=1
    else
        echo "PASS replayed packet stopped before RECEPTION"
    fi

    if grep -q "\[PKT=$DROP_PKT\].*EV=SPATEM_DELIVER_OK" "$LOG"; then
        echo "FAIL replayed packet reached DELIVER"
        FAIL=1
    else
        echo "PASS replayed packet stopped before DELIVER"
    fi
fi

if [[ "$RUN_RC" -ne 0 ]]; then
    FAIL=1
fi

echo
if [[ "$FAIL" -eq 0 ]]; then
    echo "RESULTADO 9.9 REAL REPLAY ACCEPTANCE: PASS"
    exit 0
else
    echo "RESULTADO 9.9 REAL REPLAY ACCEPTANCE: FAIL"
    exit 1
fi
