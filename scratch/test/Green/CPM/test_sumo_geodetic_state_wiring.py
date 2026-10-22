#!/usr/bin/env python3

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]

MOBILITY = ROOT / (
    "scratch/vanetza_integration/context/"
    "vehicle_mobility/vehicle_mobility_context.h"
)
PARSER = ROOT / "scratch/vanetza_integration/sumo_adapter/sumo_parser.cc"
UPDATER = ROOT / (
    "scratch/vanetza_integration/context/"
    "vehicle_state_updater/vehicle_state_updater.cc"
)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def check(label, condition):
    print(f"{'PASS' if condition else 'FAIL'} {label}")
    return condition


def main():
    mobility = strip_comments(MOBILITY.read_text())
    parser = strip_comments(PARSER.read_text())
    updater = strip_comments(UPDATER.read_text())

    results = []

    results.append(check(
        "GEODETIC_POSITION_PAIR_TYPE",
        bool(re.search(
            r"struct\s+VehicleGeodeticPosition\s*\{"
            r"[^}]*double\s+longitudeDeg\s*\{?\s*\}?\s*;"
            r"[^}]*double\s+latitudeDeg\s*\{?\s*\}?\s*;",
            mobility,
            re.DOTALL,
        )),
    ))

    results.append(check(
        "OPTIONAL_GEODETIC_POSITION_IN_MOBILITY",
        bool(re.search(
            r"std::optional\s*<\s*VehicleGeodeticPosition\s*>\s*"
            r"geodeticPosition\b",
            mobility,
        )),
    ))

    both_coordinates_checked = (
        "datagram->longitudeDeg.has_value()" in parser
        and "datagram->latitudeDeg.has_value()" in parser
    )
    pair_assigned = bool(re.search(
        r"vehicle->mobility\.geodeticPosition\s*=\s*"
        r"VehicleGeodeticPosition\s*\{\s*"
        r"\*datagram->longitudeDeg\s*,\s*"
        r"\*datagram->latitudeDeg\s*\}",
        parser,
        re.DOTALL,
    ))

    results.append(check(
        "SUMO_PARSER_MAPS_COMPLETE_WGS84_PAIR",
        both_coordinates_checked and pair_assigned,
    ))

    results.append(check(
        "GEODETIC_AUDIT_IS_OPT_IN_AND_POST_UPDATE",
        "V2X_GEO_AUDIT" in updater
        and "[VEHICLE_STATE_GEODETIC_APPLIED]" in updater
        and "state.mobility.geodeticPosition" in updater
        and updater.find("state.mobility = source.mobility;")
            < updater.find("[VEHICLE_STATE_GEODETIC_APPLIED]"),
    ))

    results.append(check(
        "UPDATER_COPIES_MOBILITY_CONTEXT",
        bool(re.search(
            r"state\.mobility\s*=\s*source\.mobility\s*;",
            updater,
        )),
    ))

    passed = all(results)
    print(
        "SUMO_GEODETIC_STATE_WIRING="
        + ("PASS" if passed else "FAIL")
    )
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
