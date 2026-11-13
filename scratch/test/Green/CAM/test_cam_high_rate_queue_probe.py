import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[4]
TRANSMISSION = (
    ROOT / "scratch/vanetza_integration/cam/transmission/cam_transmission.cc"
)
SCHEDULER = (
    ROOT / "scratch/vanetza_integration/cam/scheduler/cam_scheduler.cc"
)


class TestCamHighRateQueueProbe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.transmission = TRANSMISSION.read_text()
        cls.scheduler = SCHEDULER.read_text()

    def test_high_rate_mode_is_opt_in(self):
        self.assertIn("V2X_CAM_HIGH_RATE_TEST", self.transmission)
        self.assertIn("std::getenv", self.transmission)

    def test_high_rate_mode_is_not_unconditionally_enabled(self):
        self.assertRegex(
            self.transmission,
            r"HighRateTestEnabled\s*\(\s*\)\s*\{",
        )
        self.assertNotRegex(
            self.transmission,
            r"constexpr\s+bool\s+CAM_HIGH_RATE_TEST\s*=\s*true",
        )

    def test_high_rate_generation_is_bounded_per_vehicle(self):
        self.assertRegex(
            self.transmission,
            r"CAM_TEST_MAX_PACKETS_PER_VEHICLE\s*=\s*40",
        )
        self.assertRegex(
            self.transmission,
            r"vehicle\.camSequenceCounter\s*>=\s*"
            r"CAM_TEST_MAX_PACKETS_PER_VEHICLE",
        )

    def test_high_rate_branch_uses_runtime_switch(self):
        self.assertIn("if (HighRateTestEnabled())", self.transmission)

    def test_normal_scheduler_cadence_is_preserved(self):
        self.assertIn("return MilliSeconds(10);", self.scheduler)

    def test_experimental_interval_remains_one_millisecond(self):
        self.assertRegex(
            self.transmission,
            r"CAM_TEST_INTERVAL\s*=\s*0\.001",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
