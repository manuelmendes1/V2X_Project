from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate

    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestTxCompletion(unittest.TestCase):
    def test_terminal_callback_is_invoked_at_most_once(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        sources = [
            root / "scratch/test/Green/Context/test_tx_completion.cc",
            root / "scratch/vanetza_integration/context/tx_bridge/tx_completion.cc",
        ]

        with tempfile.TemporaryDirectory(
            prefix="tx-completion-"
        ) as temp:
            binary = Path(temp) / "test_tx_completion"

            command = [
                compiler,
                "-std=c++23",
                "-pthread",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                f"-I{root / 'scratch'}",
                *[str(source) for source in sources],
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
                    "TX_COMPLETION_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            print("TX_COMPLETION_BUILD=PASS")

            run = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                timeout=30,
            )

            output = run.stdout + run.stderr
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                f"TxCompletion falhou, RC={run.returncode}",
            )
            self.assertIn("TX_COMPLETION=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
