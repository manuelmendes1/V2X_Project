#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "scratch" / "CMakeLists.txt").is_file()
            and (candidate / "scratch" / "vanetza_integration").is_dir()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada")


def cmake_cache_value(cache_path: Path, key: str) -> str:
    prefix = key + ":"
    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()
    raise RuntimeError(f"{key} nao encontrado em {cache_path}")


class TestVamClusterOperationValidation(unittest.TestCase):
    def test_cluster_operation_validation_contract(self):
        root = repository_root(Path(__file__))
        cache = root / "cmake-cache" / "CMakeCache.txt"

        compiler = Path(cmake_cache_value(cache, "CMAKE_CXX_COMPILER"))
        if not compiler.is_file():
            self.fail(f"Compilador configurado nao encontrado: {compiler}")

        test_source = Path(__file__).with_suffix(".cc")
        implementation = (
            root / "scratch/vanetza_integration/vam/cluster/"
            "vam_cluster_operation_validation.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="vam-cluster-operation-validation-"
        ) as temp:
            binary = Path(temp) / "test_vam_cluster_operation_validation"

            command = [
                str(compiler),
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
                check=False,
                timeout=60,
            )

            self.assertEqual(
                build.returncode,
                0,
                msg=(
                    "VAM_CLUSTER_OPERATION_VALIDATION_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
                ),
            )

            print(f"VAM_CLUSTER_OPERATION_VALIDATION_COMPILER={compiler}")
            print("VAM_CLUSTER_OPERATION_VALIDATION_BUILD=PASS")

            run = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
                timeout=20,
            )

            output = run.stdout + run.stderr
            if output:
                print(output, end="")

            self.assertEqual(run.returncode, 0, msg=output)
            self.assertIn("VAM_CLUSTER_OPERATION_VALIDATION=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
