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


class TestSumoVehicleRemoveDatagram(unittest.TestCase):
    def test_vehicle_remove_datagram_contract(self):
        root = repository_root(Path(__file__))
        source = (
            root / "scratch/test/Red/VAM/"
                 "test_sumo_vehicle_remove_datagram.cc"
        )
        implementation = (
            root / "scratch/vanetza_integration/sumo_adapter/"
                     "sumo_vehicle_datagram.cc"
        )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        with tempfile.TemporaryDirectory(
            prefix="sumo-vehicle-remove-datagram-"
        ) as temp:
            binary = Path(temp) / "test_sumo_vehicle_remove_datagram"
            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                str(source),
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
                "SUMO_VEHICLE_REMOVE_DATAGRAM_BUILD=FAIL\n"
                f"Command: {' '.join(command)}\n"
                f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}",
            )

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
            self.assertIn("SUMO_VEHICLE_REMOVE_DATAGRAM=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
