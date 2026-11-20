#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
CXX="${CXX:-g++}"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/v2x-c02.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

command -v "$CXX" >/dev/null 2>&1 || {
  echo "ERROR: C++ compiler not found: $CXX" >&2
  exit 2
}

SCRATCH="$ROOT/scratch"
COMMON=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -I"$SCRATCH")

echo "===== C-02 / AID SECURITY PROFILE BUILD ====="
"$CXX" "${COMMON[@]}" \
  "$SCRATCH/test/Red/Security/test_v2x_v3_security_profile.cc" \
  -o "$TMP_DIR/test_v2x_v3_security_profile"

echo "===== C-02 / AID SECURITY PROFILE RUNTIME ====="
"$TMP_DIR/test_v2x_v3_security_profile"

echo "===== C-02 / V3 SECURITY CONTRACT ====="
python3 "$SCRATCH/test/Red/Security/test_v3_security_contract.py"

echo "===== C-02 / CPM SECURITY WIRING REGRESSION ====="
python3 "$SCRATCH/test/Red/CPM/test_cpm_security_wiring.py"

echo "===== C-02 / SECURITY FAULT-INJECTION REGRESSION ====="
python3 "$SCRATCH/test/Red/CPM/test_cpm_security_fault_injection.py"

echo "C-02_VALIDATION=PASS"
