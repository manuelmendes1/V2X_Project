import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def find_repository_root(start):
    resolved = start.resolve()

    for candidate in (resolved, *resolved.parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate

    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestCpmNeutralModelContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="cpm-neutral-model-contract-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_cpm_neutral_model_contract"

        compiler = shutil.which("g++")
        if not compiler:
            cls.tempdir.cleanup()
            raise RuntimeError("Não foi encontrado o compilador g++.")

        source = (
            cls.root
            / "scratch/test/Green/CPM/"
            "test_cpm_neutral_model_contract.cc"
        )
        if not source.is_file():
            cls.tempdir.cleanup()
            raise RuntimeError(
                f"Não foi encontrado o teste C++: {source}"
            )

        command = [
            compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{cls.root / 'scratch'}",
            str(source),
            "-o",
            str(cls.binary),
        ]

        result = subprocess.run(
            command,
            cwd=cls.root,
            capture_output=True,
            text=True,
            check=False,
        )

        if result.returncode != 0:
            cls.tempdir.cleanup()
            raise RuntimeError(
                "Compilação do contrato neutro CPM falhou.\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        print("CPM_NEUTRAL_MODEL_CONTRACT_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_neutral_model_contract(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

        output = result.stdout + result.stderr
        if output:
            print(output, end="")

        self.assertEqual(
            result.returncode,
            0,
            msg=(
                "O contrato do modelo neutro CPM falhou.\n"
                f"Exit code: {result.returncode}\n"
                f"Saída:\n{output}"
            ),
        )
        print("CPM_NEUTRAL_MODEL_CONTRACT=PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
