import unittest
from pathlib import Path


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestCpmEmptyPerceptionSkip(unittest.TestCase):
    def test_empty_perception_is_skipped_before_runtime_and_send(self):
        root = repository_root(Path(__file__))
        source_path = (
            root / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        )
        source = source_path.read_text(encoding="utf-8")

        start = source.find(
            "SimulationController::RunCpmPerception("
            "std::uint64_t referenceTimeUs)"
        )
        end = source.find(
            "SimulationController::DrainCpmReceptionQueues()",
            start,
        )

        self.assertGreaterEqual(
            start, 0, "RUN_CPM_PERCEPTION_NOT_FOUND"
        )
        self.assertGreater(
            end, start, "RUN_CPM_PERCEPTION_END_NOT_FOUND"
        )

        method = source[start:end]

        guard = method.find("if (input.perceivedObjects.empty())")
        build_accepted = method.find(
            '"[CPM_PERCEPTION] BUILD_ACCEPTED"'
        )
        ensure_runtime = method.find("EnsureCpmRuntime()")
        send = method.find("m_cpmRuntime->Send(")

        checks = {
            "EMPTY_LIST_GUARD_PRESENT":
                guard >= 0,
            "EMPTY_LIST_GUARD_BEFORE_BUILD_ACCEPTED":
                guard >= 0
                and build_accepted >= 0
                and guard < build_accepted,
            "EMPTY_LIST_SKIP_REASON_PRESENT":
                guard >= 0
                and build_accepted > guard
                and "reason=NO_PERCEIVED_OBJECTS"
                in method[guard:build_accepted],
            "EMPTY_LIST_RETURNS_BEFORE_TX_READY":
                guard >= 0
                and build_accepted > guard
                and "return;" in method[guard:build_accepted],
            "EMPTY_LIST_SKIP_BEFORE_RUNTIME_INITIALIZATION":
                guard >= 0
                and ensure_runtime > guard,
            "EMPTY_LIST_SKIP_BEFORE_SEND":
                guard >= 0
                and send > guard,
        }

        for name, passed in checks.items():
            print(f'{"PASS" if passed else "FAIL"} {name}')

        missing = [
            name for name, passed in checks.items() if not passed
        ]
        self.assertEqual(
            missing,
            [],
            "CPM_EMPTY_PERCEPTION_SKIP_MISSING="
            + ",".join(missing),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
