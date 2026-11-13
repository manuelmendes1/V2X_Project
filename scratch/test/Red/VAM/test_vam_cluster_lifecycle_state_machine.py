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


class TestVamClusterLifecycleStateMachine(unittest.TestCase):
    def test_cluster_lifecycle_contract(self):
        root = repository_root(Path(__file__))

        header = (
            root
            / "scratch/vanetza_integration/vam/cluster/"
              "vam_cluster_lifecycle_state_machine.h"
        )
        implementation = (
            root
            / "scratch/vanetza_integration/vam/cluster/"
              "vam_cluster_lifecycle_state_machine.cc"
        )
        validator = (
            root
            / "scratch/vanetza_integration/vam/cluster/"
              "vam_cluster_operation_validation.cc"
        )

        missing = [
            str(path.relative_to(root))
            for path in (header, implementation, validator)
            if not path.is_file()
        ]

        self.assertFalse(
            missing,
            "VAM_CLUSTER_LIFECYCLE_IMPLEMENTATION_MISSING: "
            + ", ".join(missing),
        )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        test_source = (
            root
            / "scratch/test/Red/VAM/"
              "test_vam_cluster_lifecycle_state_machine.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="vam-cluster-lifecycle-"
        ) as temp:
            binary = Path(temp) / "test_vam_cluster_lifecycle_state_machine"

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
                str(validator),
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
                "VAM_CLUSTER_LIFECYCLE_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{build.stdout}\n"
                f"STDERR:\n{build.stderr}",
            )

            print("VAM_CLUSTER_LIFECYCLE_BUILD=PASS")

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
                "VAM_CLUSTER_LIFECYCLE_STATE_MACHINE=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
