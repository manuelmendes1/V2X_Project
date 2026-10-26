import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


class TestCpmRsuShutdownOrder(unittest.TestCase):
    def test_cpm_runtime_is_released_before_simulator_destroy(self):
        main = (ROOT / "scratch/v2x/v2x_sumo.cc").read_text()
        header = (
            ROOT
            / "scratch/vanetza_integration/context/simulation_controller/"
              "simulation_controller.h"
        ).read_text()
        controller = (
            ROOT
            / "scratch/vanetza_integration/context/simulation_controller/"
              "simulation_controller.cc"
        ).read_text()

        public_section = header.split("private:", 1)[0]
        self.assertIn(
            "static void ShutdownCpmEmitter();",
            public_section,
            "CPM_SHUTDOWN_MUST_BE_PUBLIC",
        )

        run_position = main.find("Simulator::Run();")
        destroy_position = main.find("Simulator::Destroy();", run_position)
        shutdown_position = main.find(
            "SimulationController::ShutdownCpmEmitter();", run_position
        )

        self.assertGreaterEqual(run_position, 0, "SIMULATOR_RUN_MISSING")
        self.assertGreaterEqual(
            shutdown_position, 0, "CPM_SHUTDOWN_CALL_MISSING"
        )
        self.assertGreater(
            destroy_position, run_position, "SIMULATOR_DESTROY_MISSING"
        )
        self.assertLess(
            shutdown_position,
            destroy_position,
            "CPM_SHUTDOWN_MUST_PRECEDE_SIMULATOR_DESTROY",
        )

        body_start = controller.find(
            "SimulationController::ShutdownCpmEmitter()"
        )
        self.assertGreaterEqual(body_start, 0, "CPM_SHUTDOWN_IMPLEMENTATION_MISSING")
        body = controller[body_start:controller.find("\n}\n", body_start)]
        runtime_reset = body.find("m_cpmRuntime.reset();")
        endpoint_reset = body.find("m_cpmEmitter.reset();")

        self.assertGreaterEqual(runtime_reset, 0, "CPM_RUNTIME_NOT_RELEASED")
        self.assertGreaterEqual(endpoint_reset, 0, "CPM_ENDPOINT_NOT_RELEASED")
        self.assertLess(
            runtime_reset,
            endpoint_reset,
            "CPM_RUNTIME_MUST_BE_RELEASED_BEFORE_ENDPOINT",
        )


if __name__ == "__main__":
    unittest.main()
