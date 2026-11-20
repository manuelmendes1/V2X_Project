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


def read_cache_value(cache_path, key):
    prefix = key + ":"

    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()

    raise RuntimeError(f"Variavel {key} nao encontrada no CMakeCache.")


class TestSumoGeodeticTransform(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))

        cache = cls.root / "cmake-cache" / "CMakeCache.txt"
        if not cache.is_file():
            raise RuntimeError(f"CMakeCache nao encontrado: {cache}")

        cls.compiler = Path(
            read_cache_value(cache, "CMAKE_CXX_COMPILER")
        )
        cls.geographiclib_root = Path(
            read_cache_value(cache, "V2X_GEOGRAPHICLIB_ROOT")
        )

        if not cls.compiler.is_file():
            raise RuntimeError(f"Compilador inexistente: {cls.compiler}")

        library = cls.geographiclib_root / "lib" / "libGeographicLib.so"
        include = cls.geographiclib_root / "include"

        if not library.is_file():
            raise RuntimeError(f"GeographicLib inexistente: {library}")

        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="sumo-geodetic-transform-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_sumo_geodetic_transform"

        test_source = (
            cls.root / "scratch/test/Red/VAM/"
            "test_sumo_geodetic_transform.cc"
        )

        implementation = (
            cls.root / "scratch/vanetza_integration/sumo_adapter/"
            "sumo_geodetic_transform.cc"
        )

        header = (
            cls.root / "scratch/vanetza_integration/sumo_adapter/"
            "sumo_geodetic_transform.h"
        )

        if not header.is_file() or not implementation.is_file():
            raise RuntimeError(
                "SUMO_GEODETIC_TRANSFORM_API_MISSING: "
                "RED esperado; header e implementacao de producao ainda "
                "nao existem."
            )

        command = [
            str(cls.compiler),
            "-std=c++23",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{cls.root / 'scratch'}",
            f"-I{include}",
            str(test_source),
            str(implementation),
            f"-L{cls.geographiclib_root / 'lib'}",
            f"-Wl,-rpath,{cls.geographiclib_root / 'lib'}",
            "-lGeographicLib",
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
            raise RuntimeError(
                "SUMO_GEODETIC_TRANSFORM_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        print("SUMO_GEODETIC_TRANSFORM_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tempdir"):
            cls.tempdir.cleanup()

    def test_geodetic_transform_contract(self):
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
            msg=f"Contrato geodetico falhou:\n{output}",
        )
        self.assertIn("SUMO_GEODETIC_TRANSFORM=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
