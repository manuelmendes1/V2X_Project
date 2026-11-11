import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
INTEGRATION = ROOT / "scratch/vanetza_integration"


class TestCamSchedulerGeodeticGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scheduler = (
            INTEGRATION / "cam/scheduler/cam_scheduler.cc"
        ).read_text()
        cls.builder = (
            INTEGRATION / "cam/builder/cam_builder.cc"
        ).read_text()

    def test_geodetic_position_checked_before_sequence_reservation(self):
        callback_start = self.scheduler.find(
            "VehicleDispatcher::ForEachReadyVehicle"
        )
        self.assertGreaterEqual(callback_start, 0)

        callback = self.scheduler[callback_start:]
        prepare = callback.find(
            "CamTransmission::PrepareTransmission"
        )
        self.assertGreaterEqual(prepare, 0)

        optional_check = callback.find(
            "vehicle.mobility.geodeticPosition.has_value()"
        )
        self.assertGreaterEqual(
            optional_check,
            0,
            "Scheduler does not reject a missing geodetic position.",
        )

        validator_check = callback.find(
            "CamValidator::ValidateWgs84"
        )
        self.assertGreaterEqual(
            validator_check,
            0,
            "Scheduler does not validate WGS84 before transmission.",
        )

        self.assertLess(
            optional_check,
            prepare,
            "Missing-position check occurs after sequence reservation.",
        )
        self.assertLess(
            validator_check,
            prepare,
            "WGS84 validation occurs after sequence reservation.",
        )

        print(
            "[PASS] scheduler validates geodetic position "
            "before PrepareTransmission"
        )

    def test_builder_uses_wgs84_not_projected_mobility(self):
        latitude_assignment = re.search(
            r"ctx\.camInput\.latitude\s*=\s*[^;]*latitudeDeg\s*;",
            self.builder,
        )
        longitude_assignment = re.search(
            r"ctx\.camInput\.longitude\s*=\s*[^;]*longitudeDeg\s*;",
            self.builder,
        )

        self.assertIsNotNone(
            latitude_assignment,
            "CAM latitude is not sourced from latitudeDeg.",
        )
        self.assertIsNotNone(
            longitude_assignment,
            "CAM longitude is not sourced from longitudeDeg.",
        )

        self.assertNotRegex(
            self.builder,
            r"ctx\.camInput\.latitude\s*=\s*vehicle\.mobility\.position\.",
        )
        self.assertNotRegex(
            self.builder,
            r"ctx\.camInput\.longitude\s*=\s*vehicle\.mobility\.position\.",
        )

        print(
            "[PASS] builder uses geodetic latitudeDeg/longitudeDeg"
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
