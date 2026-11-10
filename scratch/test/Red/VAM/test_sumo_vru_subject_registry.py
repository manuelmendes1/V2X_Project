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


class TestSumoVruSubjectRegistry(unittest.TestCase):
    def test_registry_contract(self):
        root = repository_root(Path(__file__))

        header = (
            root
            / "scratch/vanetza_integration/context/"
              "sumo_vru_subject_registry/sumo_vru_subject_registry.h"
        )
        implementation = (
            root
            / "scratch/vanetza_integration/context/"
              "sumo_vru_subject_registry/sumo_vru_subject_registry.cc"
        )

        missing = [
            str(path)
            for path in (header, implementation)
            if not path.is_file()
        ]

        self.assertFalse(
            missing,
            "SUMO_VRU_SUBJECT_REGISTRY_API_MISSING: "
            + ", ".join(missing),
        )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        sources = [
            root / "scratch/test/Red/VAM/test_sumo_vru_subject_registry.cc",
            implementation,
            root
            / "scratch/vanetza_integration/vru/role/state_machine/"
              "vru_role_state_machine.cc",
            root
            / "scratch/vanetza_integration/vru/role/policy/"
              "vru_role_policy.cc",
        ]

        with tempfile.TemporaryDirectory(
            prefix="sumo-vru-subject-registry-"
        ) as temp:
            binary = Path(temp) / "test_sumo_vru_subject_registry"

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                *[str(source) for source in sources],
                "-o",
                str(binary),
            ]

            build = subprocess.run(
                command,
                cwd=root,
                capture_output=True,
                text=True,
                timeout=60,
            )

            self.assertEqual(
                build.returncode,
                0,
                "SUMO_VRU_SUBJECT_REGISTRY_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{build.stdout}\n"
                f"STDERR:\n{build.stderr}",
            )

            print("SUMO_VRU_SUBJECT_REGISTRY_BUILD=PASS")

            run = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                timeout=20,
            )

            output = run.stdout + run.stderr
            print(output, end="")

            self.assertEqual(run.returncode, 0, output)
            self.assertIn("SUMO_VRU_SUBJECT_REGISTRY=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
