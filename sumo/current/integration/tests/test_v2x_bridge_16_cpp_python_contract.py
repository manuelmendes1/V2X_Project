import sys
import unittest
from dataclasses import fields
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[4]
INTEGRATION_DIR = PROJECT_ROOT / "sumo/current/integration"

sys.path.insert(0, str(INTEGRATION_DIR))

import v2x_bridge_16 as bridge


class TestCppPythonVehicleStateContract(unittest.TestCase):
    EXPECTED_FIELDS = (
        "vehicle_id",
        "x",
        "y",
        "speed",
        "heading",
        "accel",
    )

    CPP_EXPRESSIONS = (
        "vehicle.identity.vehId",
        "vehicle.mobility.position.x",
        "vehicle.mobility.position.y",
        "vehicle.mobility.speed",
        "vehicle.mobility.heading",
        "vehicle.mobility.accel",
    )

    def test_python_dataclass_preserves_wire_field_order(self):
        actual_fields = tuple(
            field.name for field in fields(bridge.Ns3VehicleState)
        )

        self.assertEqual(actual_fields, self.EXPECTED_FIELDS)

    def test_cpp_serializer_preserves_exact_wire_field_order(self):
        serializer_path = (
            PROJECT_ROOT
            / "scratch/vanetza_integration/sumo_adapter/sumo_serializer.cc"
        )

        source = serializer_path.read_text(encoding="utf-8")

        method_start = source.find("SumoSerializer::Serialize")
        self.assertNotEqual(
            method_start,
            -1,
            "C++ serializer implementation was not found",
        )

        method_end = source.find("\n}", method_start)
        self.assertNotEqual(
            method_end,
            -1,
            "C++ serializer method boundary was not found",
        )

        implementation = source[method_start:method_end]

        positions = [
            implementation.find(expression)
            for expression in self.CPP_EXPRESSIONS
        ]

        self.assertTrue(
            all(position >= 0 for position in positions),
            "One or more expected VehicleState fields are missing",
        )

        self.assertEqual(
            positions,
            sorted(positions),
            "C++ serializer field order differs from the Python contract",
        )

        self.assertEqual(
            implementation.count(' + "," + '),
            5,
            "C++ serializer must emit exactly six CSV fields",
        )

        self.assertEqual(
            implementation.count("std::to_string("),
            5,
            "C++ serializer must encode exactly five numeric fields",
        )

    def test_python_parser_maps_each_wire_position_correctly(self):
        state = bridge.parse_ns3_vehicle_state(
            "contract-probe,101.0,202.0,303.0,404.0,505.0"
        )

        actual = (
            state.vehicle_id,
            state.x,
            state.y,
            state.speed,
            state.heading,
            state.accel,
        )

        expected = (
            "contract-probe",
            101.0,
            202.0,
            303.0,
            404.0,
            505.0,
        )

        self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
