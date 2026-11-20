#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import v2x_bridge_16 as bridge


class TestV2XBridge16PersonVClass(unittest.TestCase):
    def test_serializes_sumo_vehicle_class_as_optional_source_metadata(self):
        message = bridge.serialize_person_upsert(
            person_id="ped0",
            x=10.125,
            y=20.250,
            speed=1.4,
            angle_deg=90.0,
            simulation_time_us=1234567,
            sumo_vclass="pedestrian",
        )
        self.assertEqual(
            message,
            "SUMO_PERSON,UPSERT,ped0,10.125,20.250,1.400,90.000,1234567,pedestrian",
        )

    def test_preserves_non_pedestrian_class_without_profile_inference(self):
        message = bridge.serialize_person_upsert(
            "person-bike", 1.0, 2.0, 1.0, 90.0, 10,
            sumo_vclass="bicycle",
        )
        self.assertTrue(message.endswith(",bicycle"))

    def test_rejects_csv_unsafe_or_non_string_class(self):
        for sumo_vclass in ("pedestrian,extra", "pedestrian\ninjected", 7, ""):
            with self.subTest(sumo_vclass=sumo_vclass):
                with self.assertRaises(ValueError):
                    bridge.serialize_person_upsert(
                        "ped0", 1.0, 2.0, 1.0, 90.0, 10,
                        sumo_vclass=sumo_vclass,
                    )

    def test_reads_person_vehicle_class_and_publishes_it(self):
        sock = Mock()
        with (
            patch.object(bridge.traci.person, "getVehicleClass", return_value="pedestrian") as get_class,
            patch.object(bridge.traci.person, "getPosition", return_value=(10.125, 20.250)),
            patch.object(bridge.traci.person, "getSpeed", return_value=1.4),
            patch.object(bridge.traci.person, "getAngle", return_value=90.0),
        ):
            bridge.send_person_to_ns3(sock, "ped0", 1234567)

        get_class.assert_called_once_with("ped0")
        sock.sendto.assert_called_once_with(
            b"SUMO_PERSON,UPSERT,ped0,10.125,20.250,1.400,90.000,1234567,pedestrian",
            ("127.0.0.1", 7400),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
