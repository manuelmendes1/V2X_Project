import subprocess
import tempfile
import unittest
from pathlib import Path


def find_repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do ns-3 nao encontrada.")


def read_cmake_cache_value(cache_path, key):
    prefix = key + ":"

    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()

    raise RuntimeError(f"Variavel {key} nao encontrada em {cache_path}")


class TestSumoSpatemIntersectionBuilderBoundary(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="spatem-sumo-boundary-"
        )
        cls.binary = (
            Path(cls.tempdir.name)
            / "test_sumo_spatem_intersection_builder_boundary"
        )
        cls.link_map = Path(cls.tempdir.name) / "spatem-link.map"

        cache_path = cls.root / "cmake-cache" / "CMakeCache.txt"
        compiler = Path(
            read_cmake_cache_value(cache_path, "CMAKE_CXX_COMPILER")
        )
        vanetza_root = Path(
            read_cmake_cache_value(cache_path, "V2X_VANETZA_ROOT")
        )

        archive = vanetza_root / "lib" / "libvanetza_common.a"
        include = vanetza_root / "include"

        for required in (compiler, archive, include):
            if not required.exists():
                raise RuntimeError(
                    f"Dependencia do toolchain nao encontrada: {required}"
                )

        integration = cls.root / "scratch/vanetza_integration"

        test_source = (
            cls.root / "scratch/test/Red/SPATEM/"
            "test_sumo_spatem_intersection_builder_boundary.cc"
        )

        sources = [
            integration / "sumo_adapter/"
            "sumo_traffic_light_spatem_intersection_builder.cc",
            integration / "sumo_adapter/"
            "sumo_traffic_light_spatem_mapper.cc",
            integration / "sumo_adapter/"
            "sumo_traffic_light_mapper.cc",
            integration / "sumo_adapter/"
            "sumo_traffic_light_timing_mapper.cc",
            integration / "context/its_time/"
            "its_start_time_parser.cc",
        ]

        missing = [
            path for path in [test_source, *sources]
            if not path.is_file()
        ]
        if missing:
            raise RuntimeError(
                "Fontes necessarias em falta:\n"
                + "\n".join(str(path) for path in missing)
            )

        command = [
            str(compiler),
            "-std=c++23",
            "-O0",
            "-g",
            f"-I{cls.root / 'scratch'}",
            f"-I{integration}",
            f"-I{include}",
            str(test_source),
            *(str(source) for source in sources),
            f"-Wl,-Map={cls.link_map}",
            "-Wl,--start-group",
            str(archive),
            "-Wl,--end-group",
            "-lboost_date_time",
            "-o", str(cls.binary),
        ]

        compiled = subprocess.run(
            command,
            cwd=cls.root,
            capture_output=True,
            text=True,
            check=False,
            timeout=120,
        )

        if compiled.returncode != 0:
            raise RuntimeError(
                "Compilacao do teste C++ falhou.\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{compiled.stdout}\n"
                f"STDERR:\n{compiled.stderr}"
            )

        if not cls.link_map.is_file():
            raise RuntimeError("Mapa de ligacao nao foi gerado.")

        map_content = cls.link_map.read_text(
            encoding="utf-8", errors="replace"
        )

        if str(archive) not in map_content:
            raise RuntimeError(
                "Biblioteca Vanetza de producao ausente do mapa de ligacao."
            )

        print(f"SPATEM_BOUNDARY_COMPILER={compiler}")
        print(f"SPATEM_BOUNDARY_VANETZA={vanetza_root}")
        print("SPATEM_BOUNDARY_LINK_MAP=PASS")
        print("SPATEM_BOUNDARY_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_link_index_boundary_and_state_preservation(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
            timeout=20,
        )

        self.assertEqual(
            result.returncode,
            0,
            msg=f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}",
        )
        self.assertIn(
            "SPATEM_SUMO_BOUNDARY_TESTS=PASS", result.stdout
        )
        self.assertIn(
            "SPATEM_SUMO_BOUNDARY_FAILED=0", result.stdout
        )
        print(result.stdout, end="")


if __name__ == "__main__":
    unittest.main(verbosity=2)
