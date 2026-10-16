from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[4]
SOURCE = (
    ROOT
    / "scratch/vanetza_integration/context/simulation_controller/"
      "simulation_controller.cc"
)


class TestSpatemCamInitialPhaseOffset(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        start = cls.source.index("// CAM passa a ter um scheduler independente.")
        end = cls.source.index(
            "// DENM é explicitamente opt-in",
            start,
        )
        cls.start_block = cls.source[start:end]

    def test_cam_initial_phase_offset_is_opt_in(self):
        self.assertIn("V2X_CAM_INITIAL_DELAY_US", self.start_block)

    def test_zero_offset_preserves_immediate_cam_start(self):
        self.assertIn("Simulator::ScheduleNow(", self.start_block)

    def test_nonzero_offset_delays_only_initial_scheduler_start(self):
        self.assertIn("MicroSeconds(", self.start_block)
        self.assertIn("&SimulationController::SendCam", self.start_block)

    def test_periodic_cam_cadence_remains_unchanged(self):
        self.assertIn(
            "CamScheduler::RunInterval()",
            self.source,
        )


if __name__ == "__main__":
    unittest.main()
