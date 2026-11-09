#!/usr/bin/env python3

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "scratch" / "test" / "run_tests.py").is_file()
            and (candidate / "scratch" / "vanetza_integration").is_dir()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestSumoVruProfileMapper(unittest.TestCase):
    def test_sumo_vclass_mapping_contract(self):
        root = repository_root(Path(__file__))
        header = root / "scratch/vanetza_integration/vru/profile/sumo_vru_profile_mapper.h"
        implementation = root / "scratch/vanetza_integration/vru/profile/sumo_vru_profile_mapper.cc"
        test_source = root / "scratch/test/Red/VAM/test_sumo_vru_profile_mapper.cc"

        missing = [str(path) for path in (header, implementation) if not path.is_file()]
        self.assertFalse(
            missing,
            "SUMO_VRU_PROFILE_MAPPER_API_MISSING: " + ", ".join(missing),
        )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        with tempfile.TemporaryDirectory(prefix="sumo-vru-profile-") as temp:
            binary = Path(temp) / "test_sumo_vru_profile_mapper"
            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                str(test_source),
                str(implementation),
                "-o",
                str(binary),
            ]
            build = subprocess.run(
                command, cwd=root, capture_output=True, text=True, timeout=60
            )
            self.assertEqual(
                build.returncode,
                0,
                "SUMO_VRU_PROFILE_MAPPER_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}",
            )
            print("SUMO_VRU_PROFILE_MAPPER_BUILD=PASS")

            run = subprocess.run(
                [str(binary)], cwd=root, capture_output=True, text=True, timeout=20
            )
            output = run.stdout + run.stderr
            print(output, end="")
            self.assertEqual(run.returncode, 0, output)
            self.assertIn("SUMO_VRU_PROFILE_MAPPER=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
