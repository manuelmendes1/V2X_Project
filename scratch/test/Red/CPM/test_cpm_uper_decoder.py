import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate

    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestCpmUperDecoder(unittest.TestCase):
    def test_rsu_roundtrip_and_malformed_payload_rejection(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++")

        self.assertIsNotNone(compiler, "g++ não encontrado")

        integration = root / "scratch/vanetza_integration"

        test_source = (
            root / "scratch/test/Red/CPM/test_cpm_uper_decoder.cc"
        )
        decoder_source = (
            integration / "cpm/codec/cpm_uper_decoder.cc"
        )
        encoder_source = (
            integration / "cpm/codec/cpm_uper_encoder.cc"
        )
        quantizer_source = (
            integration / "cpm/perception/cpm_coordinate_quantizer.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="cpm-uper-decoder-"
        ) as temporary:
            binary = Path(temporary) / "test_cpm_uper_decoder"

            command = [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                f"-I{root / 'scratch'}",
                f"-I{root / 'build/include'}",
                "-I/usr/local/include",
                "-I/usr/local/include/vanetza/asn1/support",
                str(test_source),
                str(decoder_source),
                str(encoder_source),
                str(quantizer_source),
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
                check=False,
            )

            if build.returncode != 0:
                self.fail(
                    "Compilação do teste CPM UPER Decoder falhou.\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            result = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
            )

            output = result.stdout + result.stderr
            print(output, end="")

            self.assertEqual(result.returncode, 0, msg=output)
            self.assertIn("CPM_UPER_DECODER_CONTRACT=PASS", output)
            self.assertIn("CPM_UPER_DECODER_FAILED=0", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
