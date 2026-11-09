#!/usr/bin/env python3

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
HEADER = ROOT / "scratch/vanetza_integration/context/simulation_controller/simulation_controller.h"
CONTROLLER = ROOT / "scratch/vanetza_integration/context/simulation_controller/simulation_controller.cc"
CMAKE = ROOT / "scratch/CMakeLists.txt"


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        return ""
    opening = source.find("{", start)
    if opening < 0:
        return ""

    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    return ""


class TestSumoPersonControllerWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text()
        cls.controller = CONTROLLER.read_text()
        cls.cmake = CMAKE.read_text()

    def test_controller_registers_person_callback(self):
        registration = function_body(
            self.controller, "SimulationController::ConfigureSumoAdapterCallbacks()"
        )
        self.assertIn("SumoAdapter::SetPersonUpdateCallback(", registration)
        self.assertIn("&SimulationController::HandlePersonUpdateFromSumo", registration)

    def test_controller_handler_applies_datagram_to_registry(self):
        handler = function_body(
            self.controller,
            "SimulationController::HandlePersonUpdateFromSumo(",
        )
        self.assertTrue(handler, "HandlePersonUpdateFromSumo não implementado")
        self.assertIn("m_sumoPersonRegistry.Apply(datagram)", handler)

    def test_registry_is_exposed_read_only_for_next_vru_stage(self):
        self.assertRegex(
            self.header,
            r"static\s+const\s+SumoPersonRegistry&\s+GetSumoPersonRegistry\s*\(\s*\)\s*;",
        )
        self.assertIn(
            "SimulationController::GetSumoPersonRegistry()", self.controller
        )
        self.assertIn("static SumoPersonRegistry m_sumoPersonRegistry", self.header)

    def test_person_registry_is_reset_at_simulation_start(self):
        start = function_body(self.controller, "SimulationController::Start(Time stepLength)")
        self.assertIn("m_sumoPersonRegistry.Clear()", start)

    def test_controller_does_not_map_person_ingress_to_vehicle_state(self):
        handler = function_body(
            self.controller,
            "SimulationController::HandlePersonUpdateFromSumo(",
        )
        self.assertTrue(handler, "HandlePersonUpdateFromSumo não implementado")
        self.assertNotIn("VehicleState", handler)
        self.assertNotIn("SimulationSync::ProcessVehicle", handler)
        self.assertNotIn("CreateObject<Node>", handler)

    def test_registry_implementation_is_part_of_production_target(self):
        self.assertIn(
            "context/sumo_person_registry/sumo_person_registry.cc", self.cmake
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
