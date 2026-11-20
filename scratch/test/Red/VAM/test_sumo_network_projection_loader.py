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


def cache_value(path, key):
    prefix = key + ":"

    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()

    raise RuntimeError(f"Variavel {key} nao encontrada no CMakeCache.")


class TestSumoNetworkProjectionLoader(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cache = cls.root / "cmake-cache" / "CMakeCache.txt"

        if not cache.is_file():
            raise RuntimeError(f"CMakeCache inexistente: {cache}")

        cls.compiler = Path(cache_value(cache, "CMAKE_CXX_COMPILER"))
        cls.geographiclib_root = Path(
            cache_value(cache, "V2X_GEOGRAPHICLIB_ROOT")
        )
        cls.xml_include = Path(cache_value(cache, "LIBXML2_INCLUDE_DIR"))
        cls.xml_library = Path(cache_value(cache, "LIBXML2_LIBRARY"))

        if not cls.compiler.is_file():
            raise RuntimeError(f"Compilador inexistente: {cls.compiler}")

        if not (cls.xml_include / "libxml" / "parser.h").is_file():
            raise RuntimeError(
                f"Header LibXml2 inexistente: {cls.xml_include}"
            )

        if not cls.xml_library.is_file():
            raise RuntimeError(f"Biblioteca LibXml2 inexistente: {cls.xml_library}")

        cls.geographiclib_library = (
            cls.geographiclib_root / "lib" / "libGeographicLib.so"
        )

        if not cls.geographiclib_library.is_file():
            raise RuntimeError(
                f"GeographicLib inexistente: {cls.geographiclib_library}"
            )

        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="sumo-network-projection-loader-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_sumo_network_projection_loader"

        loader_header = (
            cls.root / "scratch/vanetza_integration/sumo_adapter/"
            "sumo_network_projection_loader.h"
        )
        loader_source = (
            cls.root / "scratch/vanetza_integration/sumo_adapter/"
            "sumo_network_projection_loader.cc"
        )
        transform_source = (
            cls.root / "scratch/vanetza_integration/sumo_adapter/"
            "sumo_geodetic_transform.cc"
        )
        test_source = (
            cls.root / "scratch/test/Red/VAM/"
            "test_sumo_network_projection_loader.cc"
        )

        if not loader_header.is_file() or not loader_source.is_file():
            raise RuntimeError(
                "SUMO_NETWORK_PROJECTION_LOADER_API_MISSING: "
                "RED esperado; loader de produção ainda não existe."
            )

        command = [
            str(cls.compiler),
            "-std=c++23",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{cls.root / 'scratch'}",
            f"-I{cls.xml_include}",
            f"-I{cls.geographiclib_root / 'include'}",
            str(test_source),
            str(loader_source),
            str(transform_source),
            str(cls.xml_library),
            str(cls.geographiclib_library),
            f"-Wl,-rpath,{cls.geographiclib_root / 'lib'}",
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
                "SUMO_NETWORK_PROJECTION_LOADER_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        print("SUMO_NETWORK_PROJECTION_LOADER_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tempdir"):
            cls.tempdir.cleanup()

    def test_network_projection_loader_contract(self):
        result = subprocess.run(
            [str(self.binary), self.tempdir.name],
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
            msg=f"Contrato do loader falhou:\n{output}",
        )
        self.assertIn(
            "SUMO_NETWORK_PROJECTION_LOADER=PASS",
            output,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
