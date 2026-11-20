import os
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


class TestVamUperEncoder(unittest.TestCase):
    def test_profile1_uper_roundtrip_and_semantic_guards(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        vanetza_root = Path(
            os.environ.get(
                "V2X_VANETZA_ROOT",
                str(Path.home() / "opt" / "vanetza-cpm-gcc11"),
            )
        )
        r2_include = vanetza_root / "include/vanetza/asn1/its/r2"
        support_include = vanetza_root / "include/vanetza/asn1/support"

        with tempfile.TemporaryDirectory(prefix="vam-uper-encoder-") as temp:
            binary = Path(temp) / "test_vam_uper_encoder"
            test_source = root / "scratch/test/Red/VAM/test_vam_uper_encoder.cc"
            encoder_source = (
                root / "scratch/vanetza_integration/vam/codec/vam_uper_encoder.cc"
            )
            validator_source = (
                root / "scratch/vanetza_integration/vam/codec/vam_header_validator.cc"
            )

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                f"-I{vanetza_root / 'include'}",
                f"-I{r2_include}",
                f"-I{support_include}",
                str(test_source),
                str(encoder_source),
                str(validator_source),
                f"-L{vanetza_root / 'lib'}",
                f"-Wl,-rpath,{vanetza_root / 'lib'}",
                "-lvanetza_asn1_its_r2",
                "-lvanetza_asn1",
                "-lvanetza_asn1_support",
                "-o",
                str(binary),
            ]

            build = subprocess.run(
                command, cwd=root, capture_output=True, text=True, timeout=90
            )
            if build.returncode != 0:
                self.fail(
                    "VAM_UPER_ENCODER_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
                )

            print("VAM_UPER_ENCODER_BUILD=PASS")
            result = subprocess.run(
                [str(binary)], cwd=root, capture_output=True, text=True, timeout=20
            )
            output = result.stdout + result.stderr
            print(output, end="")
            self.assertEqual(result.returncode, 0, msg=output)
            self.assertIn("VAM_UPER_ENCODER_CONTRACT=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
