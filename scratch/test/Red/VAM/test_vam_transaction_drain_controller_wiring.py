#!/usr/bin/env python3

from pathlib import Path
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / ".git").exists()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("NS3_REPOSITORY_ROOT_NOT_FOUND")


def braced_block_after(source: str, marker: str) -> str:
    marker_position = source.find(marker)
    if marker_position < 0:
        raise AssertionError(f"Missing source marker: {marker}")

    opening = source.find("{", marker_position)
    if opening < 0:
        raise AssertionError(f"Missing block after marker: {marker}")

    depth = 0
    for position in range(opening, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:position]

    raise AssertionError(f"Unclosed block after marker: {marker}")


class TestVamTransactionDrainControllerWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = repository_root(Path(__file__))

        cls.controller_path = (
            root
            / "scratch/vanetza_integration/context/"
              "simulation_controller/simulation_controller.cc"
        )
        cls.header_path = (
            root
            / "scratch/vanetza_integration/context/"
              "simulation_controller/simulation_controller.h"
        )
        cls.main_path = root / "scratch/v2x/v2x_sumo.cc"

        cls.controller = cls.controller_path.read_text()
        cls.header = cls.header_path.read_text()
        cls.main = cls.main_path.read_text()

        cls.configure = braced_block_after(
            cls.controller,
            "SimulationController::ConfigureVamGeneration(",
        )
        cls.shutdown = braced_block_after(
            cls.controller,
            "SimulationController::ShutdownVamGeneration(",
        )

    def test_disable_drains_before_destroying_the_active_scheduler(self):
        self.assertTrue(self.configure)

        disabled = braced_block_after(self.configure, "if (!enabled)")
        self.assertTrue(disabled)

        drain = disabled.find("DrainTransactions")
        reset = disabled.find("m_vamGenerationScheduler.reset()")

        self.assertGreaterEqual(
            drain, 0,
            "Disable must drain VAM transactions before destroying scheduler",
        )
        self.assertGreater(reset, drain)
        self.assertIn("return false", disabled)

    def test_replacement_drains_old_scheduler_after_validation_before_swap(self):
        self.assertTrue(self.configure)

        construction = self.configure.find("replacementScheduler")
        validation = self.configure.find(
            "replacementScheduler->HasValidInstanceIdentity()"
        )
        drain = self.configure.find(
            "m_vamGenerationScheduler->DrainTransactions",
            validation,
        )
        publication = self.configure.find(
            "m_vamGenerationScheduler.swap(replacementScheduler)"
        )

        self.assertGreaterEqual(construction, 0)
        self.assertGreater(validation, construction)
        self.assertGreater(drain, validation)
        self.assertGreater(publication, drain)

        drain_guard = braced_block_after(
            self.configure,
            "if (!m_vamGenerationScheduler->DrainTransactions(error))",
        )
        self.assertTrue(drain_guard)
        self.assertIn("return false", drain_guard)

    def test_shutdown_is_failure_aware_and_preserves_scheduler_on_drain_failure(self):
        self.assertIn(
            "static bool ShutdownVamGeneration();",
            self.header,
        )
        self.assertTrue(self.shutdown)

        drain = self.shutdown.find("DrainTransactions")
        reset = self.shutdown.find("m_vamGenerationScheduler.reset()")

        self.assertGreaterEqual(drain, 0)
        self.assertGreater(reset, drain)

        guard = braced_block_after(
            self.shutdown,
            "if (!m_vamGenerationScheduler->DrainTransactions(error))",
        )
        self.assertTrue(guard)
        self.assertIn("return false", guard)
        self.assertNotIn(
            "m_vamGenerationScheduler.reset()",
            guard,
            "A failed drain must preserve the active scheduler",
        )
        self.assertIn("return true", self.shutdown)

    def test_main_blocks_normal_teardown_and_identity_clear_on_shutdown_failure(self):
        main_body = braced_block_after(self.main, "int main(")
        self.assertTrue(main_body)

        shutdown_call = "if (!SimulationController::ShutdownVamGeneration())"
        shutdown_guard = braced_block_after(main_body, shutdown_call)
        self.assertTrue(
            shutdown_guard,
            "main must check the VAM shutdown result",
        )
        self.assertIn("return", shutdown_guard)

        shutdown_position = main_body.find(shutdown_call)
        cpm_shutdown_position = main_body.find(
            "SimulationController::ShutdownCpmEmitter()"
        )
        spatem_shutdown_position = main_body.find(
            "SimulationController::ShutdownSpatemEmitter()"
        )
        destroy_position = main_body.find("Simulator::Destroy()")
        clear_position = main_body.find(
            "ItsStationIdentityAuthority::Instance().Clear()"
        )

        self.assertGreaterEqual(shutdown_position, 0)
        self.assertGreater(cpm_shutdown_position, shutdown_position)
        self.assertGreater(spatem_shutdown_position, shutdown_position)
        self.assertGreater(destroy_position, shutdown_position)
        self.assertGreater(clear_position, destroy_position)


if __name__ == "__main__":
    unittest.main(verbosity=2)
