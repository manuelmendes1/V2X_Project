from pathlib import Path
import os
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


class TestCpmRxPipeline(unittest.TestCase):
    def test_decode_semantic_delivery_and_rejection_contracts(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++") or shutil.which("g++-11")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        base = root / "scratch/vanetza_integration"
        sources = [
            root / "scratch/test/Red/CPM/test_cpm_rx_pipeline.cc",
            base / "cpm/rx/cpm_rx_pipeline.cc",
            base / "cpm/codec/cpm_uper_decoder.cc",
            base / "cpm/codec/cpm_uper_encoder.cc",
            base / "cpm/perception/cpm_coordinate_quantizer.cc",
            base / "context/packet/packet_context.cc",
            base / "context/packet/packet_payload.cc",
            base / "context/packet/packet_types.cc",
            base / "core/v2x_log.cc",
        ]

        for source in sources:
            self.assertTrue(source.is_file(), f"Ficheiro em falta: {source}")

        with tempfile.TemporaryDirectory(prefix="cpm-rx-pipeline-") as tmp:
            binary = Path(tmp) / "test_cpm_rx_pipeline"
            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                f"-I{root / 'scratch'}",
                f"-I{root / 'build/include'}",
                f"-I{root / 'cmake-cache/include'}",
                "-I/usr/local/include",
                "-I/usr/local/include/vanetza",
                "-I/usr/local/include/vanetza/asn1",
                "-I/usr/local/include/vanetza/asn1/support",
                *[str(source) for source in sources],
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
                text=True,
                capture_output=True,
                timeout=90,
            )

            if build.returncode != 0:
                self.fail(
                    "CPM_RX_PIPELINE_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            env = os.environ.copy()
            env["LD_LIBRARY_PATH"] = (
                "/usr/local/lib:" + env.get("LD_LIBRARY_PATH", "")
            )
            run = subprocess.run(
                [str(binary)],
                cwd=root,
                text=True,
                capture_output=True,
                env=env,
                timeout=30,
            )

            output = run.stdout + run.stderr
            print("CPM_RX_PIPELINE_BUILD=PASS")
            print(output, end="")

            self.assertEqual(run.returncode, 0, msg=output)
            self.assertIn("CPM_RX_PIPELINE=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
