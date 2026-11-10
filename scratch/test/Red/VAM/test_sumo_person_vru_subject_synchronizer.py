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


class TestSumoPersonVruSubjectSynchronizer(unittest.TestCase):
    def test_person_and_subject_registries_remain_consistent(self):
        root = repository_root(Path(__file__))

        source_root = root / "scratch/vanetza_integration"

        paths = [
            root / "scratch/test/Red/VAM/"
                 "test_sumo_person_vru_subject_synchronizer.cc",
            source_root / "context/sumo_person_vru_subject_synchronizer/"
                          "sumo_person_vru_subject_synchronizer.cc",
            source_root / "context/sumo_person_registry/"
                          "sumo_person_registry.cc",
            source_root / "context/sumo_vru_subject_registry/"
                          "sumo_vru_subject_registry.cc",
            source_root / "vru/profile/sumo_vru_profile_mapper.cc",
            source_root / "vru/role/state_machine/"
                         "vru_role_state_machine.cc",
            source_root / "vru/role/policy/vru_role_policy.cc",
        ]

        missing = [str(path) for path in paths if not path.is_file()]
        self.assertFalse(
            missing,
            "SUMO_PERSON_VRU_SUBJECT_SYNCHRONIZER_API_MISSING: "
            + ", ".join(missing),
        )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        with tempfile.TemporaryDirectory(
            prefix="sumo-person-vru-subject-sync-"
        ) as temp:
            binary = Path(temp) / "test_sumo_person_vru_subject_synchronizer"

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                *[str(path) for path in paths],
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
                "SUMO_PERSON_VRU_SUBJECT_SYNCHRONIZER_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{build.stdout}\n"
                f"STDERR:\n{build.stderr}",
            )

            print("SUMO_PERSON_VRU_SUBJECT_SYNCHRONIZER_BUILD=PASS")

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
            self.assertIn(
                "SUMO_PERSON_VRU_SUBJECT_SYNCHRONIZER=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
