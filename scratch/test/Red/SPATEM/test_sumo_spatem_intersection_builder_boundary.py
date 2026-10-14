import shutil
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
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


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

        compiler = shutil.which("g++-11") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado g++-11 nem g++.")

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
        ]

        command = [
            compiler,
            "-std=c++23",
            "-O0",
            "-g",
            f"-I{cls.root / 'scratch'}",
            f"-I{integration}",
            "-I/usr/local/include",
            "-I/usr/local/include/vanetza",
            str(test_source),
            *(str(source) for source in sources),
            "-L/usr/local/lib",
            "-Wl,-rpath,/usr/local/lib",
            "-lvanetza_common",
            "-lboost_date_time",
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
                "Compilação do teste C++ falhou.\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{compiled.stdout}\n"
                f"STDERR:\n{compiled.stderr}"
            )

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
        )

        self.assertEqual(
            result.returncode,
            0,
            msg=f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}",
        )
        self.assertIn(
            "SPATEM_SUMO_BOUNDARY_TESTS=PASS", result.stdout
        )
        self.assertIn("SPATEM_SUMO_BOUNDARY_FAILED=0", result.stdout)
        print(result.stdout, end="")


if __name__ == "__main__":
    unittest.main(verbosity=2)
