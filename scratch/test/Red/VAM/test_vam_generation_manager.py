#!/usr/bin/env python3

import shutil
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


class TestVamGenerationManager(unittest.TestCase):
    def test_generation_management_contract(self):
        root = repository_root(Path(__file__))
        manager_dir = root / "scratch/vanetza_integration/vam/generation"
        header = manager_dir / "vam_generation_manager.h"
        implementation_sources = [
            manager_dir / "vam_generation_manager.cc",
            root / "scratch/vanetza_integration/vam/cluster/"
            "vam_cluster_operation_validation.cc",
        ]
        test_source = Path(__file__).with_name("test_vam_generation_manager.cc")

        missing = [
            str(path.relative_to(root))
            for path in (header, *implementation_sources)
            if not path.is_file()
        ]
        if missing:
            self.fail(
                "RED_EXPECTED: gestor de geracao VAM ainda nao implementado; "
                "os testes comportamentais ja definem o contrato esperado:\n"
                + "\n".join(missing)
            )

        cache = root / "cmake-cache/CMakeCache.txt"

        if cache.is_file():
            compiler = Path(cmake_cache_value(cache, "CMAKE_CXX_COMPILER"))
        else:
            discovered_compiler = (
                shutil.which("g++-11") or shutil.which("g++")
            )
            if discovered_compiler is None:
                self.fail(
                    "CMakeCache.txt inexistente e nenhum g++-11/g++ "
                    "foi encontrado no PATH."
                )
            compiler = Path(discovered_compiler)

        if not compiler.is_file():
            self.fail(f"Compilador configurado nao encontrado: {compiler}")

        with tempfile.TemporaryDirectory(prefix="vam-generation-manager-") as temp:
            binary = Path(temp) / "test_vam_generation_manager"
            command = [
                str(compiler),
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                str(test_source),
                *(str(source) for source in implementation_sources),
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
                    "Compilacao dos testes do VamGenerationManager falhou.\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
                ),
            )

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
            self.assertIn("VAM_GENERATION_MANAGER=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
