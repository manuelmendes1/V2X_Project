#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "scratch" / "test" / "run_tests.py").is_file()
            and (candidate / "scratch" / "vanetza_integration").is_dir()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestSumoPersonDatagram(unittest.TestCase):
    def test_person_datagram_parser_contract(self):
        root = repository_root(Path(__file__))
        header = (
            root / "scratch/vanetza_integration/sumo_adapter/"
                 "sumo_person_datagram.h"
        )
        source = (
            root / "scratch/vanetza_integration/sumo_adapter/"
                 "sumo_person_datagram.cc"
        )
        test_source = root / "scratch/test/Red/SUMO/test_sumo_person_datagram.cc"

        missing = [str(path) for path in (header, source) if not path.is_file()]
        self.assertFalse(
            missing,
            "SUMO_PERSON_DATAGRAM_API_MISSING: faltam os ficheiros de produção: "
            + ", ".join(missing),
        )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11 nem g++ encontrado")

        with tempfile.TemporaryDirectory(prefix="sumo-person-datagram-") as temp:
            binary = Path(temp) / "test_sumo_person_datagram"
            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                str(test_source),
                str(source),
                "-o",
                str(binary),
            ]

            build = subprocess.run(
                command,
                cwd=root,
                capture_output=True,
                text=True,
                timeout=60,
                check=False,
            )
            if build.returncode != 0:
                self.fail(
                    "SUMO_PERSON_DATAGRAM_BUILD=FAIL\n"
                    f"Header esperado: {header}\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
                )

            print("SUMO_PERSON_DATAGRAM_BUILD=PASS")
            run = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                timeout=20,
                check=False,
            )
            output = run.stdout + run.stderr
            print(output, end="")
            self.assertEqual(run.returncode, 0, output)
            self.assertIn("SUMO_PERSON_DATAGRAM_CONTRACT=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
