#!/usr/bin/env python3

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


class TestCpmGenerationManager(unittest.TestCase):
    def test_generation_manager_contract(self):
        root = find_repository_root(Path(__file__))
        base = root / "scratch/vanetza_integration/cpm/perception"
        header = base / "cpm_generation_manager.h"
        implementation = base / "cpm_generation_manager.cc"
        test_source = (
            root / "scratch/test/Green/CPM/"
            "test_cpm_generation_manager.cc"
        )

        missing = [
            str(path.relative_to(root))
            for path in (header, implementation)
            if not path.is_file()
        ]

        if missing:
            self.fail(
                "RED_EXPECTED: gestão de geração CPM ainda não implementada:\n"
                + "\n".join(missing)
            )

        compiler = shutil.which("g++")
        if not compiler:
            self.fail("Não foi encontrado o compilador g++.")

        with tempfile.TemporaryDirectory(
            prefix="cpm-generation-manager-"
        ) as temp:
            binary = Path(temp) / "test_cpm_generation_manager"
            command = [
                compiler,
                "-std=c++17",
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
                check=False,
            )

            self.assertEqual(
                build.returncode,
                0,
                msg=(
                    "Compilação do contrato de geração CPM falhou.\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                ),
            )

            result = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
            )

            output = result.stdout + result.stderr
            if output:
                print(output, end="")

            self.assertEqual(
                result.returncode,
                0,
                msg=f"Contrato de geração CPM falhou:\n{output}",
            )
            self.assertIn("CPM_GENERATION_MANAGER=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
