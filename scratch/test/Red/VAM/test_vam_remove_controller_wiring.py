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

    raise AssertionError(f"Unclosed source block after marker: {marker}")


class TestVamRemoveControllerWiring(unittest.TestCase):
    def test_accepted_remove_invalidates_vam_before_subject_synchronization(self):
        root = repository_root(Path(__file__))
        controller_path = (
            root
            / "scratch/vanetza_integration/context/"
              "simulation_controller/simulation_controller.cc"
        )
        source = controller_path.read_text()

        handler = braced_block_after(
            source,
            "SimulationController::HandlePersonUpdateFromSumo(",
        )

        apply_marker = "if (!m_sumoPersonRegistry.Apply(datagram))"
        remove_marker = (
            "if (datagram.eventType == SumoPersonEventType::Remove)"
        )
        sync_marker = (
            "SumoVruSubjectSynchronizer::SynchronizeAccepted"
        )

        apply_position = handler.find(apply_marker)
        remove_position = handler.find(remove_marker)
        invalidation_position = handler.find(
            "InvalidatePersonPresence",
            remove_position if remove_position >= 0 else 0,
        )
        sync_position = handler.find(sync_marker)

        self.assertGreaterEqual(apply_position, 0)
        self.assertGreaterEqual(remove_position, 0)
        self.assertGreaterEqual(invalidation_position, 0)
        self.assertGreaterEqual(sync_position, 0)

        self.assertLess(apply_position, remove_position)
        self.assertLess(remove_position, invalidation_position)
        self.assertLess(invalidation_position, sync_position)

        rejection = braced_block_after(handler, apply_marker)
        self.assertNotIn(
            "InvalidatePersonPresence",
            rejection,
            "Rejected or stale REMOVE must not invalidate VAM state",
        )

        remove_block = braced_block_after(handler, remove_marker)
        self.assertIn(
            "m_vamGenerationScheduler->InvalidatePersonPresence",
            remove_block,
        )
        self.assertIn(
            "CANDIDATE_ABORT_FAILED",
            remove_block,
            "Abort failure must be reported by the controller",
        )
        self.assertNotIn(
            "return;",
            remove_block,
            "VAM abort failure must not prevent accepted REMOVE synchronization",
        )
        self.assertEqual(
            handler.count("InvalidatePersonPresence"),
            1,
            "Presence invalidation must be scoped to the accepted REMOVE path",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
