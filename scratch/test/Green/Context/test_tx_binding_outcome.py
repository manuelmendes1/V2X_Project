from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (candidate / "ns3").is_file() and (
            candidate / "scratch" / "CMakeLists.txt"
        ).is_file():
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestTxBindingOutcome(unittest.TestCase):
    def test_legacy_context_reaches_terminal_failure_and_drop(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        sources = [
            root / "scratch/test/Green/Context/test_tx_binding_outcome.cc",
            root / "scratch/vanetza_integration/context/tx_bridge/tx_binding_outcome.cc",
            root / "scratch/vanetza_integration/context/tx_bridge/tx_completion.cc",
            root / "scratch/vanetza_integration/context/packet/packet_context.cc",
        ]

        with tempfile.TemporaryDirectory(
            prefix="tx-binding-outcome-"
        ) as temp:
            binary = Path(temp) / "test_tx_binding_outcome"

            command = [
                compiler,
                "-std=c++23",
                "-pthread",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-ffunction-sections",
                "-fdata-sections",
                f"-I{root / 'scratch'}",
                f"-I{root / 'build/include'}",
                *[str(source) for source in sources],
                "-Wl,--gc-sections",
                "-o",
                str(binary),
            ]

            build = subprocess.run(
                command,
                cwd=root,
                capture_output=True,
                text=True,
                timeout=90,
            )

            if build.returncode != 0:
                self.fail(
                    "TX_BINDING_OUTCOME_BUILD=FAIL\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            print("TX_BINDING_OUTCOME_BUILD=PASS")

            run = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                timeout=30,
            )

            output = run.stdout + run.stderr
            print(output, end="")

            self.assertEqual(run.returncode, 0, output)
            self.assertIn("TX_BINDING_OUTCOME=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
