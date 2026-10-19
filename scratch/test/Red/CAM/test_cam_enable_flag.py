from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[4]

MAIN = ROOT / "scratch/v2x/v2x_sumo.cc"
HEADER = (
    ROOT
    / "scratch/vanetza_integration/context/simulation_controller/"
      "simulation_controller.h"
)
SOURCE = (
    ROOT
    / "scratch/vanetza_integration/context/simulation_controller/"
      "simulation_controller.cc"
)


class TestCamEnableFlag(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main = MAIN.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")

        start = cls.source.index(
            "// CAM passa a ter um scheduler independente."
        )
        end = cls.source.index(
            "// DENM é explicitamente opt-in",
            start,
        )
        cls.cam_start_block = cls.source[start:end]

    def test_cam_remains_enabled_by_default(self):
        self.assertRegex(
            self.main,
            r"\bbool\s+enableCam\s*=\s*true\s*;",
        )

    def test_cli_exposes_enable_cam(self):
        self.assertIn('"enableCam"', self.main)

    def test_main_applies_cam_configuration(self):
        self.assertIn(
            "SimulationController::SetCamEnabled(enableCam)",
            self.main,
        )

    def test_controller_declares_cam_configuration_api(self):
        self.assertIn("SetCamEnabled(bool enabled)", self.header)

    def test_disabled_cam_is_not_scheduled_at_start(self):
        self.assertRegex(
            self.cam_start_block,
            r"if\s*\(\s*m_camEnabled\s*\)",
        )


if __name__ == "__main__":
    unittest.main()
