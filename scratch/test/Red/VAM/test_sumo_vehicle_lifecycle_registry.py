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


class TestSumoVehicleLifecycleRegistry(unittest.TestCase):
    def test_vehicle_lifecycle_history_contract(self):
        root = repository_root(Path(__file__))
        source_root = root / "scratch/vanetza_integration"

        test_source = (
            root / "scratch/test/Red/VAM/"
                 "test_sumo_vehicle_lifecycle_registry.cc"
        )
        header = (
            source_root / "context/sumo_vehicle_lifecycle_registry/"
                          "sumo_vehicle_lifecycle_registry.h"
        )
        implementation = (
            source_root / "context/sumo_vehicle_lifecycle_registry/"
                          "sumo_vehicle_lifecycle_registry.cc"
        )

        missing = [
            str(path)
            for path in (test_source, header, implementation)
            if not path.is_file()
        ]
        self.assertFalse(
            missing,
            "SUMO_VEHICLE_LIFECYCLE_SOURCES_MISSING: "
            + ", ".join(missing),
        )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        with tempfile.TemporaryDirectory(
            prefix="sumo-vehicle-lifecycle-"
        ) as temp:
            binary = Path(temp) / "test_sumo_vehicle_lifecycle_registry"

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
                command,
                cwd=root,
                capture_output=True,
                text=True,
                timeout=60,
            )

            self.assertEqual(
                build.returncode,
                0,
                "SUMO_VEHICLE_LIFECYCLE_BUILD=FAIL\n"
                f"Command: {' '.join(command)}\n"
                f"STDOUT:\n{build.stdout}\n"
                f"STDERR:\n{build.stderr}",
            )

            print("SUMO_VEHICLE_LIFECYCLE_BUILD=PASS")

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
                "SUMO_VEHICLE_LIFECYCLE_REGISTRY=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
