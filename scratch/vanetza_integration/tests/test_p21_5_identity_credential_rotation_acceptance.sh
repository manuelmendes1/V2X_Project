#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
POLICY_LOG="$ROOT/test-results/p21/p21_5_identity_policy.log"
E2E_LOG="$ROOT/test-results/p21/p21_5_runtime_rotation.log"

fail() {
    echo "FAIL P21.5 $1"
    exit 1
}

pass() {
    echo "PASS P21.5 $1"
}

echo "============================================================"
echo "P21.5 — IDENTITY / CREDENTIAL ROTATION ACCEPTANCE"
echo "============================================================"

[[ -f "$POLICY_LOG" ]] ||
    fail "identity policy runtime log not found"

[[ -f "$E2E_LOG" ]] ||
    fail "runtime E2E rotation log not found"

echo
echo "===== IDENTITY / OWNERSHIP POLICY ====="

grep -q \
    'DENM_IDENTITY_CREDENTIAL_ACTION_POLICY_EXIT=0' \
    "$POLICY_LOG" ||
    fail "identity/credential policy runtime did not report EXIT=0"

pass "identity/credential policy runtime reported EXIT=0"

for expected in \
    'PASS rotation rejects a credential belonging to another vehicle' \
    'PASS rejected foreign credential preserves active credential index' \
    'PASS active credential still belongs to veh0 after rejection' \
    'PASS stationID is unchanged after rejected credential rotation' \
    'PASS legitimate veh0 credential rotation succeeds' \
    'PASS credential sequence 001 becomes active' \
    'PASS rotated active credential still belongs to veh0' \
    'PASS stationID remains unchanged after legitimate credential rotation'
do
    grep -Fq "$expected" "$POLICY_LOG" ||
        fail "missing policy evidence: ${expected#PASS }"
done

pass "foreign credential is rejected without changing active identity"
pass "legitimate rotation selects another credential of the same vehicle"
pass "stationID remains invariant across rejection and legitimate rotation"

echo
echo "===== ACTIVE DENM LIFECYCLE / ACTIONID ====="

grep -q \
    'PASS ActionID origin equals authoritative stationID' \
    "$POLICY_LOG" ||
    fail "ActionID is not bound to authoritative stationID"

grep -q \
    'PASS first ActionID has a valid sequence' \
    "$POLICY_LOG" ||
    fail "first ActionID sequence is invalid"

grep -q \
    'PASS credential rotation does not change the active lifecycle ActionID' \
    "$POLICY_LOG" ||
    fail "credential rotation changed the active lifecycle ActionID"

grep -q \
    'PASS independent DENM event receives a different ActionID sequence' \
    "$POLICY_LOG" ||
    fail "independent DENM event did not receive a new ActionID sequence"

pass "ActionID origin is authoritative stationID"
pass "credential rotation preserves the active lifecycle ActionID"
pass "independent DENM event receives a distinct ActionID"

echo
echo "===== RUNTIME E2E ROTATION ====="

grep -q \
    'DENM_V3_RUNTIME_E2E_CREDENTIAL_ROTATION_EXIT=0' \
    "$E2E_LOG" ||
    fail "runtime E2E rotation did not report EXIT=0"

pass "runtime E2E credential rotation reported EXIT=0"

for expected in \
    'PASS first DENM exposes the real V3 signer certificate identity' \
    'PASS runtime credential rotation succeeds between DENM transmissions' \
    'PASS credential 001 is active before second DENM' \
    'PASS credential 001 certificate is active before second DENM' \
    'PASS second DENM exposes the real V3 signer certificate identity' \
    'PASS second DENM is an UPDATE of the existing lifecycle event' \
    'PASS DENM UPDATE preserves the same ActionID' \
    'PASS DENM lifecycle preserves stationID across credential rotation' \
    'PASS first DENM TX_COMPLETE result is OK' \
    'PASS second DENM TX_COMPLETE result is OK' \
    'PASS DENM signer identity changes after runtime credential rotation'
do
    grep -Fq "$expected" "$E2E_LOG" ||
        fail "missing E2E evidence: ${expected#PASS }"
done

first_signer="$(
    sed -n 's/^FIRST_SIGNER_CERT_ID=\(.*\)$/\1/p' "$E2E_LOG" |
    head -n 1
)"

second_signer="$(
    sed -n 's/^SECOND_SIGNER_CERT_ID=\(.*\)$/\1/p' "$E2E_LOG" |
    head -n 1
)"

[[ -n "$first_signer" ]] ||
    fail "FIRST_SIGNER_CERT_ID is missing"

[[ -n "$second_signer" ]] ||
    fail "SECOND_SIGNER_CERT_ID is missing"

[[ "$first_signer" != "$second_signer" ]] ||
    fail "signer certificate identity did not change after rotation"

echo "first_signer=$first_signer"
echo "second_signer=$second_signer"

pass "new DENM transmission uses a different signer certificate"
pass "DENM UPDATE preserves ActionID and stationID across rotation"
pass "both DENM transmissions complete with TX_RESULT=OK"

echo
echo "============================================================"
echo "P21_5_IDENTITY_CREDENTIAL_ROTATION_ACCEPTANCE_EXIT=0"
echo "============================================================"
