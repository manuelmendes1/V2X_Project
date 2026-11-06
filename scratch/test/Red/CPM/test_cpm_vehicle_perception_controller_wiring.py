#!/usr/bin/env python3

import re
import unittest

from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]

HEADER = ROOT / "scratch/vanetza_integration/context/simulation_controller/simulation_controller.h"
CONTROLLER = ROOT / "scratch/vanetza_integration/context/simulation_controller/simulation_controller.cc"
MAIN = ROOT / "scratch/v2x/v2x_sumo.cc"
CMAKE = ROOT / "scratch/CMakeLists.txt"


def main():
    files = {
        "header": HEADER.read_text(),
        "controller": CONTROLLER.read_text(),
        "main": MAIN.read_text(),
        "cmake": CMAKE.read_text(),
    }

    controller = files["controller"]
    main_source = files["main"]
    header = files["header"]
    cmake = files["cmake"]

    checks = {
        "CONTROLLER_PUBLIC_CONFIGURATION":
            "ConfigureCpmPerception(" in header
            and "SimulationController::ConfigureCpmPerception(" in controller,

        "CONTROLLER_RUNTIME_METHOD":
            "RunCpmPerception(std::uint64_t referenceTimeUs)" in header
            and "SimulationController::RunCpmPerception(std::uint64_t referenceTimeUs)" in controller,

        "SOURCE_AND_DISPATCHER_INCLUDES":
            "cpm/perception/cpm_vehicle_perception_source.h" in controller
            and "context/vehicle_dispatcher/vehicle_dispatcher.h" in controller,

        "EXPLICIT_ITS_CLOCK_CONFIGURATION":
            "SimulationContext::itsStartTime" in controller
            and "ItsStartTimeParser::ParseUtc(" in controller,

        "VEHICLE_STATE_DISPATCH":
            "VehicleDispatcher::ForEachReadyVehicle(" in controller
            and "vehicle.identity.vehId" in controller,

        "GEODETIC_POSITION_MAPPING":
            "vehicle.mobility.geodeticPosition" in controller
            and "longitudeDeg" in controller
            and "latitudeDeg" in controller,

        "SUMO_TIMESTAMP_MAPPING":
            "vehicle.sumoSimulationTimeUs" in controller,

        "CPM_SOURCE_BUILD_CALL":
            re.search(r"(?:\.|->)Build\s*\(", controller) is not None
            and "CpmInput" in controller,

        "CPM_RUN_ON_SUMO_STEP_BOUNDARY":
            (
                "SumoAdapter::SetStepCompleteCallback(" in controller
                and "RunCpmPerception(referenceTimeUs)" in controller
                and "SumoAdapter::Poll();" in controller
                and "RunCpmPerception();" not in controller
            ),

        "CPM_OPT_IN_DISABLED_BY_DEFAULT":
            re.search(r"\bbool\s+enableCpm\s*=\s*false\s*;", main_source)
            is not None,

        "EXPLICIT_CPM_CLI_CONFIGURATION":
            all(
                name in main_source
                for name in (
                    '"enableCpm"',
                    '"cpmStationId"',
                    '"cpmLatitudeDeg"',
                    '"cpmLongitudeDeg"',
                    '"cpmPerceptionRangeMeters"',
                    "SimulationController::ConfigureCpmPerception(",
                )
            ),

        "CPM_LIBRARY_LINKED":
            "vanetza_cpm_perception_obj" in cmake
            and "$<TARGET_OBJECTS:vanetza_cpm_perception_obj>" in cmake,
    }

    for name, passed in checks.items():
        print(f'{"PASS" if passed else "FAIL"} {name}')

    failed = [name for name, passed in checks.items() if not passed]

    if failed:
        print("CPM_CONTROLLER_WIRING=RED_EXPECTED")
        print("MISSING_CONTRACTS=" + ",".join(failed))
        return 1

    print("CPM_CONTROLLER_WIRING=PASS")
    return 0


class TestCpmVehiclePerceptionControllerWiring(unittest.TestCase):
    def test_controller_wiring_contract(self):
        self.assertEqual(main(), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
