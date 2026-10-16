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


class TestSumoTrafficLightMapper(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="spatem-sumo-mapper-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_sumo_mapper"

        compiler = shutil.which("g++-11") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado g++-11 nem g++.")

        integration = cls.root / "scratch/vanetza_integration"
        test_source = (
            cls.root
            / "scratch/test/Green/SPATEM/Mapper/"
            "test_sumo_traffic_light_mapper.cc"
        )
        mapper_source = (
            integration / "sumo_adapter/sumo_traffic_light_mapper.cc"
        )

        command = [
            compiler,
            "-std=c++23",
            "-O0",
            "-g",
            f"-I{cls.root / 'scratch'}",
            f"-I{integration}",
            str(test_source),
            str(mapper_source),
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

    def test_mapping_contract(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

        output = result.stdout + result.stderr
        print(output, end="")

        self.assertEqual(result.returncode, 0, msg=output)
        self.assertIn("SPATEM_SUMO_MAPPER_TESTS=PASS", output)
        self.assertIn("SPATEM_SUMO_MAPPER_FAILED=0", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
