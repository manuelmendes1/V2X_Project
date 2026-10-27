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


class TestCpmService(unittest.TestCase):
    def test_service_encodes_valid_input_and_preserves_tx_context(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        sources = [
            root / "scratch/test/Green/CPM/test_cpm_service.cc",
            root / "scratch/vanetza_integration/cpm/service/cpm_service.cc",
            root / "scratch/vanetza_integration/cpm/codec/cpm_uper_encoder.cc",
            root / "scratch/vanetza_integration/cpm/perception/cpm_coordinate_quantizer.cc",
            root / "scratch/vanetza_integration/context/packet/packet_context.cc",
            root / "scratch/vanetza_integration/context/packet/packet_payload.cc",
            root / "scratch/vanetza_integration/context/packet/packet_types.cc",
            root / "scratch/vanetza_integration/context/tx_bridge/tx_bridge_context.cc",
            root / "scratch/vanetza_integration/core/v2x_log.cc",
        ]

        with tempfile.TemporaryDirectory(
            prefix="cpm-service-"
        ) as temp:
            binary = Path(temp) / "test_cpm_service"

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                f"-I{root / 'scratch'}",
                f"-I{root / 'build/include'}",
                "-I/usr/local/include",
                "-I/usr/local/include/vanetza",
                "-I/usr/local/include/vanetza/asn1",
                "-I/usr/local/include/vanetza/asn1/support",
                *[str(source) for source in sources],
                f"-L{root / 'build/lib'}",
                f"-Wl,-rpath,{root / 'build/lib'}",
                "-lns3.47-network-default",
                "-lns3.47-core-default",
                "-L/usr/local/lib",
                "-Wl,-rpath,/usr/local/lib",
                "-lvanetza_asn1_its_r2",
                "-lvanetza_asn1",
                "-lvanetza_asn1_support",
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
                    "CPM_SERVICE_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            print("CPM_SERVICE_BUILD=PASS")

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
                f"Serviço CPM falhou, RC={run.returncode}",
            )
            self.assertIn("CPM_SERVICE=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
