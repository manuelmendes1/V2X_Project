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


class TestCpmUperEncoder(unittest.TestCase):
    def test_rsu_roundtrip_and_semantic_guards(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        with tempfile.TemporaryDirectory(prefix="cpm-uper-encoder-") as temp:
            binary = Path(temp) / "test_cpm_uper_encoder"
            test_source = (
                root / "scratch/test/Red/CPM/test_cpm_uper_encoder.cc"
            )
            encoder_source = (
                root / "scratch/vanetza_integration/cpm/codec/"
                "cpm_uper_encoder.cc"
            )
            quantizer_source = (
                root / "scratch/vanetza_integration/cpm/perception/"
                "cpm_coordinate_quantizer.cc"
            )

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
                command, cwd=root, capture_output=True, text=True
            )
            if build.returncode != 0:
                print("CPM_UPER_ENCODER_BUILD=FAIL")
                self.fail(
                    "Compilação do teste CPM UPER falhou.\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
                )

            print("CPM_UPER_ENCODER_BUILD=PASS")
            result = subprocess.run(
                [str(binary)], cwd=root, capture_output=True, text=True
            )
            output = result.stdout + result.stderr
            print(output, end="")
            self.assertEqual(result.returncode, 0, msg=output)

            for marker in (
                "VALID_RSU_INPUT_ENCODED",
                "UPER_ROUNDTRIP_DECODE",
                "RSU_HEADER_PRESERVED",
                "REFERENCE_TIME_PRESERVED",
                "WGS84_REFERENCE_POSITION_QUANTIZED",
                "UNKNOWN_ALTITUDE_PRESERVED",
                "UNKNOWN_REFERENCE_CONFIDENCE_PRESERVED",
                "RSU_ORIGIN_CONTAINER_PRESENT",
                "PERCEIVED_OBJECT_CONTAINER_PRESENT",
                "OBJECT_ID_TIME_DELTA_AND_AGE_PRESERVED",
                "ENU_POSITION_AND_UNKNOWN_CONFIDENCE_PRESERVED",
                "OBJECT_AGE_ABOVE_1500_REJECTED",
                "ZERO_STATION_ID_REJECTED",
                "NONFINITE_POSITION_REJECTED",
                "ENCODED_UPER_SIZE_BOUNDARY_SWEEP",
                "ENCODED_UPER_SIZE_LIMIT_ENFORCED",
                "SEGMENTATION_EXPLICITLY_REJECTED_BY_PROFILE",
                "DUPLICATE_OBJECT_ID_REJECTED_BY_ENCODER",
                "CPM_UPER_ENCODER_CONTRACT=PASS",
            ):
                expected_line = (
                    marker
                    if marker.startswith("CPM_UPER_ENCODER_CONTRACT=")
                    else f"PASS {marker}"
                )
                self.assertIn(expected_line, output, msg=output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
