import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def find_repository_root(start):
    resolved = Path(start).resolve()

    for candidate in resolved.parents:
        if (
            (candidate / ".git").exists()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate

    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestVamHeaderValidator(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.vanetza_root = (
            Path.home() / "opt" / "vanetza-cpm-gcc11"
        )

        cls.r2_include = (
            cls.vanetza_root / "include/vanetza/asn1/its/r2"
        )
        cls.support_include = (
            cls.vanetza_root / "include/vanetza/asn1/support"
        )

        required_headers = [
            cls.r2_include / "ItsPduHeaderVam.h",
            cls.r2_include / "MessageId.h",
            cls.support_include / "asn_application.h",
        ]

        missing = [
            str(path) for path in required_headers if not path.is_file()
        ]
        if missing:
            raise RuntimeError(
                "Headers ASN.1 R2 não encontrados:\n" +
                "\n".join(missing)
            )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado g++-11 nem g++.")

        integration = cls.root / "scratch/vanetza_integration"
        test_source = (
            cls.root / "scratch/test/Red/VAM/"
            "test_vam_header_validator.cc"
        )
        validator_source = (
            integration / "vam/codec/vam_header_validator.cc"
        )

        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="vam-header-validator-"
        )
        cls.binary = (
            Path(cls.tempdir.name) / "test_vam_header_validator"
        )

        command = [
            compiler,
            "-std=c++23",
            "-O0",
            "-g",
            "-Wall",
            "-Wextra",
            f"-I{cls.root / 'scratch'}",
            f"-I{integration}",
            f"-I{cls.r2_include}",
            f"-I{cls.support_include}",
            str(test_source),
            str(validator_source),
            "-o",
            str(cls.binary),
        ]

        compiled = subprocess.run(
            command,
            cwd=cls.root,
            capture_output=True,
            text=True,
            check=False,
        )

        if compiled.returncode != 0:
            raise RuntimeError(
                "Compilação do teste VAM falhou.\n"
                f"Prefixo ASN.1: {cls.vanetza_root}\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{compiled.stdout}\n"
                f"STDERR:\n{compiled.stderr}"
            )

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tempdir"):
            cls.tempdir.cleanup()

    def test_vam_header_semantic_constraints(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(
            result.returncode,
            0,
            msg=f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}",
        )
        self.assertIn("VAM_HEADER_VALIDATION_CASES=4", result.stdout)
        self.assertIn("VAM_HEADER_VALIDATION_PASSED=4", result.stdout)
        self.assertIn("VAM_HEADER_VALIDATION_TESTS=PASS", result.stdout)

        print(result.stdout, end="")


if __name__ == "__main__":
    unittest.main(verbosity=2)
