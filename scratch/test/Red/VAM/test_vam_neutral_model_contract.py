import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "scratch" / "test" / "run_tests.py").is_file()
            and (candidate / "scratch" / "vanetza_integration").is_dir()
        ):
            return candidate

    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada")


class TestVamNeutralModelContract(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.compiler = shutil.which("g++")

        if not cls.compiler:
            raise RuntimeError("Compilador g++ nao encontrado")

        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="vam-neutral-model-contract-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_vam_neutral_model_contract"

        cls.source = (
            cls.root
            / "scratch/test/Red/VAM/"
            "test_vam_neutral_model_contract.cc"
        )

        command = [
            cls.compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{cls.root / 'scratch'}",
            str(cls.source),
            "-o",
            str(cls.binary),
        ]

        result = subprocess.run(
            command,
            cwd=cls.root,
            capture_output=True,
            text=True,
            check=False,
            timeout=60,
        )

        if result.returncode != 0:
            cls.tempdir.cleanup()
            raise RuntimeError(
                "VAM_NEUTRAL_MODEL_CONTRACT_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        print(f"VAM_NEUTRAL_MODEL_COMPILER={cls.compiler}")
        print("VAM_NEUTRAL_MODEL_CONTRACT_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tempdir"):
            cls.tempdir.cleanup()

    def test_neutral_model_contract(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
            timeout=20,
        )

        output = result.stdout + result.stderr
        if output:
            print(output, end="")

        self.assertEqual(
            result.returncode,
            0,
            msg=(
                "Contrato do modelo neutro VAM falhou.\n"
                f"Exit code: {result.returncode}\n"
                f"Saida:\n{output}"
            ),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
