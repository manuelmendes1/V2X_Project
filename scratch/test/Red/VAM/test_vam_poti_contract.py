import subprocess
import tempfile
import unittest
from pathlib import Path


def find_repository_root(start):
    resolved = start.resolve()

    for candidate in (resolved, *resolved.parents):
        if (
            (candidate / "scratch" / "CMakeLists.txt").is_file()
            and (candidate / ".git").exists()
        ):
            return candidate

    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada.")


def read_cmake_cache_value(cache_path, key):
    prefix = key + ":"

    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()

    raise RuntimeError(
        f"Variavel {key} nao encontrada em {cache_path}"
    )


class TestVamPotiContract(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))

        cache_path = cls.root / "cmake-cache" / "CMakeCache.txt"
        if not cache_path.is_file():
            raise RuntimeError(
                f"CMakeCache nao encontrado: {cache_path}"
            )

        cls.compiler = Path(
            read_cmake_cache_value(
                cache_path, "CMAKE_CXX_COMPILER"
            )
        )

        if not cls.compiler.is_file():
            raise RuntimeError(
                f"Compilador configurado nao encontrado: {cls.compiler}"
            )

        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="vam-poti-contract-"
        )
        cls.binary = (
            Path(cls.tempdir.name) / "test_vam_poti_contract"
        )

        test_source = (
            cls.root / "scratch/test/Red/VAM/"
            "test_vam_poti_contract.cc"
        )

        implementation_sources = [
            cls.root / "scratch/vanetza_integration/vam/poti/"
            "vam_poti_validation.cc",
            cls.root / "scratch/vanetza_integration/vam/poti/"
            "vam_poti_temporal_transform.cc",
        ]

        command = [
            str(cls.compiler),
            "-std=c++23",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{cls.root / 'scratch'}",
            str(test_source),
            *(str(source) for source in implementation_sources),
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
                "VAM_POTI_CONTRACT_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        print(f"VAM_POTI_CONTRACT_COMPILER={cls.compiler}")
        print("VAM_POTI_CONTRACT_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tempdir"):
            cls.tempdir.cleanup()

    def test_poti_contract(self):
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
                "O contrato PoTi VAM falhou.\n"
                f"Exit code: {result.returncode}\n"
                f"Saida:\n{output}"
            ),
        )

        self.assertIn("VAM_POTI_CONTRACT=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
