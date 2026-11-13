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


class TestTxPacketBindingRegistry(unittest.TestCase):
    def test_packet_bindings_are_preserved_transferred_and_drained(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        sources = [
            root / "scratch/test/Green/Context/test_tx_packet_binding_registry.cc",
            root / "scratch/vanetza_integration/context/tx_bridge/tx_packet_binding_registry.cc",
            root / "scratch/vanetza_integration/context/tx_bridge/tx_completion.cc",
        ]

        with tempfile.TemporaryDirectory(
            prefix="tx-packet-binding-registry-"
        ) as temp:
            binary = Path(temp) / "test_tx_packet_binding_registry"
            command = [
                compiler, "-std=c++23", "-pthread",
                "-Wall", "-Wextra", "-Wpedantic",
                f"-I{root / 'scratch'}",
                f"-I{root / 'build/include'}",
                *[str(source) for source in sources],
                "-o", str(binary),
            ]

            build = subprocess.run(
                command, cwd=root, capture_output=True,
                text=True, timeout=90,
            )
            if build.returncode != 0:
                self.fail(
                    "TX_PACKET_BINDING_REGISTRY_BUILD=FAIL\n"
                    f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
                )

            print("TX_PACKET_BINDING_REGISTRY_BUILD=PASS")
            run = subprocess.run(
                [str(binary)], cwd=root, capture_output=True,
                text=True, timeout=30,
            )
            output = run.stdout + run.stderr
            print(output, end="")
            self.assertEqual(run.returncode, 0, output)
            self.assertIn("TX_PACKET_BINDING_REGISTRY=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
