#!/usr/bin/env bash
set -euo pipefail

LOG="${1:-test-results/p21/p21_1_lifecycle_ns3.log}"

if [[ ! -f "$LOG" ]]; then
    echo "FAIL P21.1 lifecycle acceptance: log not found: $LOG"
    exit 1
fi

echo "============================================================"
echo "P21.1 — DENM LIFECYCLE ACCEPTANCE"
echo "============================================================"

echo
echo "===== LOG ====="
echo "$LOG"

fail() {
    echo "FAIL P21.1 $1"
    exit 1
}

pass() {
    echo "PASS P21.1 $1"
}

echo
echo "===== TEST EXIT ====="

grep -q \
    'DENM_V3_LIFECYCLE_E2E_EXIT=0' \
    "$LOG" ||
    fail "runtime test did not report EXIT=0"

pass "runtime test reported EXIT=0"

echo
echo "===== LOGICAL LIFECYCLE REQUESTS ====="

new_lines="$(
    grep -E \
        'EV=REQUEST.*action=NEW.*event_id=' \
        "$LOG" || true
)"

update_lines="$(
    grep -E \
        'EV=LIFECYCLE_REQUEST.*action=1.*event_id=' \
        "$LOG" || true
)"

cancellation_lines="$(
    grep -E \
        'EV=LIFECYCLE_REQUEST.*action=2.*event_id=' \
        "$LOG" || true
)"

[[ "$(printf '%s\n' "$new_lines" | grep -c 'EV=REQUEST' || true)" -eq 1 ]] ||
    fail "expected exactly one NEW logical lifecycle request"

[[ "$(printf '%s\n' "$update_lines" | grep -c 'EV=LIFECYCLE_REQUEST' || true)" -eq 1 ]] ||
    fail "expected exactly one UPDATE logical lifecycle request"

[[ "$(printf '%s\n' "$cancellation_lines" | grep -c 'EV=LIFECYCLE_REQUEST' || true)" -eq 1 ]] ||
    fail "expected exactly one CANCELLATION logical lifecycle request"

pass "exactly one NEW request observed"
pass "exactly one UPDATE request observed"
pass "exactly one CANCELLATION request observed"

echo
echo "===== LOGICAL EVENT IDENTITY ====="

event_ids="$(
    printf '%s\n%s\n%s\n' \
        "$new_lines" \
        "$update_lines" \
        "$cancellation_lines" |
    sed -n 's/.*event_id=\([^ ]*\).*/\1/p' |
    sort -u
)"

[[ "$(printf '%s\n' "$event_ids" | sed '/^$/d' | wc -l)" -eq 1 ]] ||
    fail "NEW/UPDATE/CANCELLATION do not share one logical event_id"

event_id="$(printf '%s\n' "$event_ids" | sed -n '1p')"

echo "event_id=$event_id"
pass "NEW/UPDATE/CANCELLATION share one logical event"

echo
echo "===== ACTIONID IDENTITY ====="

action_seqs="$(
    printf '%s\n%s\n%s\n' \
        "$new_lines" \
        "$update_lines" \
        "$cancellation_lines" |
    sed -n 's/.*action_seq=\([0-9][0-9]*\).*/\1/p' |
    sort -u
)"

[[ "$(printf '%s\n' "$action_seqs" | sed '/^$/d' | wc -l)" -eq 1 ]] ||
    fail "NEW/UPDATE/CANCELLATION do not share one ActionID sequence"

action_seq="$(printf '%s\n' "$action_seqs" | sed -n '1p')"

[[ "$action_seq" != "0" ]] ||
    fail "logical ActionID sequence is zero"

echo "action_seq=$action_seq"
pass "same ActionID sequence preserved across NEW/UPDATE/CANCELLATION"

station_ids="$(
    printf '%s\n%s\n%s\n' \
        "$new_lines" \
        "$update_lines" \
        "$cancellation_lines" |
    sed -n 's/.*station=\([0-9][0-9]*\).*/\1/p' |
    sort -u
)"

[[ "$(printf '%s\n' "$station_ids" | sed '/^$/d' | wc -l)" -eq 1 ]] ||
    fail "origin station changed across lifecycle"

station_id="$(printf '%s\n' "$station_ids" | sed -n '1p')"

[[ "$station_id" != "0" ]] ||
    fail "origin station ID is zero"

echo "originating_station=$station_id"
pass "same originating station preserved across lifecycle"

echo
echo "===== GENERATION / ACTIONID PROPAGATION ====="

for phase in NEW UPDATE CANCELLATION; do
    matches="$(
        grep "REASON=P21_1_${phase}" "$LOG" |
        grep 'EV=GENERATE_OK' || true
    )"

    [[ "$(printf '%s\n' "$matches" | grep -c 'EV=GENERATE_OK' || true)" -eq 1 ]] ||
        fail "${phase} did not produce exactly one GENERATE_OK"

    generated_action_seq="$(
        printf '%s\n' "$matches" |
        sed -n 's/.*action_seq=\([0-9][0-9]*\).*/\1/p' |
        sort -u
    )"

    [[ "$generated_action_seq" == "$action_seq" ]] ||
        fail "${phase} GENERATE_OK does not preserve ActionID sequence"

    pass "${phase} generation preserves ActionID=${action_seq}"
done

echo
echo "===== PHYSICAL TRANSMISSION IDENTITY ====="

tx_lines="$(
    grep 'EV=TX_COMPLETE' "$LOG" || true
)"

tx_count="$(
    printf '%s\n' "$tx_lines" |
    grep -c 'EV=TX_COMPLETE' || true
)"

[[ "$tx_count" -eq 3 ]] ||
    fail "expected exactly 3 TX_COMPLETE events, got $tx_count"

pass "exactly three physical lifecycle transmissions completed"

tx_packets="$(
    printf '%s\n' "$tx_lines" |
    sed -n 's/.*\[PKT=\([0-9][0-9]*\)\].*/\1/p' |
    sort -n
)"

unique_tx_packets="$(
    printf '%s\n' "$tx_packets" |
    sort -u
)"

[[ "$(printf '%s\n' "$unique_tx_packets" | sed '/^$/d' | wc -l)" -eq 3 ]] ||
    fail "physical lifecycle transmissions do not have three distinct packet IDs"

echo "TX packets:"
printf '%s\n' "$unique_tx_packets"

pass "NEW/UPDATE/CANCELLATION use distinct physical packet IDs"

echo
echo "===== TX_COMPLETE RESULT ====="

bad_tx="$(
    printf '%s\n' "$tx_lines" |
    grep -v 'STAGE=TX_COMPLETE.*RESULT=OK.*EV=TX_COMPLETE' || true
)"

[[ -z "$bad_tx" ]] ||
    fail "one or more lifecycle TX_COMPLETE events are not OK"

pass "all lifecycle TX_COMPLETE events have RESULT=OK"

echo
echo "===== RECEIVER ACTIONID EVIDENCE ====="

rx_lifecycle="$(
    grep 'EV=DENM_DECODE_OK' "$LOG" |
    grep "action_seq=${action_seq}" |
    grep "origin_station=${station_id}" || true
)"

[[ -n "$rx_lifecycle" ]] ||
    fail "no receiver DENM_DECODE_OK evidence with expected origin station and ActionID"

rx_count="$(
    printf '%s\n' "$rx_lifecycle" |
    grep -c 'EV=DENM_DECODE_OK' || true
)"

echo "receiver lifecycle decode observations=$rx_count"
pass "receiver decodes lifecycle packets with expected origin station and ActionID"

echo
echo "===== RECEIVER DELIVERY EVIDENCE ====="

rx_delivery="$(
    grep 'EV=DENM_DELIVER_OK' "$LOG" |
    grep "action_seq=${action_seq}" |
    grep "sender_station=${station_id}" || true
)"

[[ -n "$rx_delivery" ]] ||
    fail "no receiver DENM_DELIVER_OK evidence with expected origin station and ActionID"

rx_delivery_count="$(
    printf '%s\n' "$rx_delivery" |
    grep -c 'EV=DENM_DELIVER_OK' || true
)"

echo "receiver lifecycle delivery observations=$rx_delivery_count"
pass "receiver delivers DENM lifecycle packets with expected origin station and ActionID"

echo
echo "===== TERMINATION ====="

grep -q \
    'PASS CANCELLATION terminates the active logical event' \
    "$LOG" ||
    fail "active logical event was not terminated"

pass "CANCELLATION terminates the active logical event"

echo
echo "===== PHYSICAL DUPLICATION CHECK ====="

duplicate_pkt="$(
    printf '%s\n' "$tx_packets" |
    sort |
    uniq -d || true
)"

[[ -z "$duplicate_pkt" ]] ||
    fail "same physical PacketContext packet ID produced multiple TX_COMPLETE events"

pass "no duplicate TX_COMPLETE for the same physical packet ID"

echo
echo "============================================================"
echo "P21_1_LIFECYCLE_ACCEPTANCE_EXIT=0"
echo "============================================================"
