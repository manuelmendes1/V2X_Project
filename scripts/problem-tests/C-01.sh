#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
CXX="${CXX:-g++}"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/v2x-c01.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

command -v "$CXX" >/dev/null 2>&1 || {
  echo "ERROR: C++ compiler not found: $CXX" >&2
  exit 2
}
test -f /usr/include/GeographicLib/UTMUPS.hpp || {
  echo "ERROR: install libgeographiclib-dev before running C-01 tests." >&2
  exit 2
}
pkg-config --exists libxml-2.0 || {
  echo "ERROR: install libxml2-dev and pkg-config before running C-01 tests." >&2
  exit 2
}

SCRATCH="$ROOT/scratch"
INTEGRATION="$SCRATCH/vanetza_integration"
COMMON=(-std=c++23 -Wall -Wextra -Wpedantic -Werror -I"$SCRATCH")

echo "===== C-01 / GEODETIC RESOLVER BUILD ====="
"$CXX" "${COMMON[@]}" \
  "$SCRATCH/test/Red/VAM/test_sumo_geodetic_transform.cc" \
  "$INTEGRATION/sumo_adapter/sumo_geodetic_transform.cc" \
  -lGeographicLib \
  -o "$TMP_DIR/test_sumo_geodetic_transform"

echo "===== C-01 / GEODETIC RESOLVER RUNTIME ====="
"$TMP_DIR/test_sumo_geodetic_transform"

echo "===== C-01 / NETWORK PROJECTION LOADER BUILD ====="
read -r -a XML_CFLAGS <<<"$(pkg-config --cflags libxml-2.0)"
read -r -a XML_LIBS <<<"$(pkg-config --libs libxml-2.0)"
"$CXX" "${COMMON[@]}" "${XML_CFLAGS[@]}" \
  "$SCRATCH/test/Red/VAM/test_sumo_network_projection_loader.cc" \
  "$INTEGRATION/sumo_adapter/sumo_network_projection_loader.cc" \
  "$INTEGRATION/sumo_adapter/sumo_geodetic_transform.cc" \
  "${XML_LIBS[@]}" -lGeographicLib \
  -o "$TMP_DIR/test_sumo_network_projection_loader"

echo "===== C-01 / NETWORK PROJECTION LOADER RUNTIME ====="
"$TMP_DIR/test_sumo_network_projection_loader" "$TMP_DIR"

echo "===== C-01 / POSITION CONSUMER CONTRACT ====="
python3 "$SCRATCH/test/Red/Context/test_unified_geodetic_position_contract.py"

echo "===== C-01 / SUMO GEODETIC STATE REGRESSION ====="
python3 "$SCRATCH/test/Green/CPM/test_sumo_geodetic_state_wiring.py"

echo "C-01_VALIDATION=PASS"
