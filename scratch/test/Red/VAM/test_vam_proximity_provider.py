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


class TestVamProximityProvider(unittest.TestCase):
    def test_proximity_provider_contract(self):
        root = repository_root(Path(__file__))
        provider_dir = (
            root / "scratch/vanetza_integration/vam/perception"
        )

        header = provider_dir / "vam_proximity_provider.h"
        implementation = provider_dir / "vam_proximity_provider.cc"
        test_source = Path(__file__).with_suffix(".cc")

        missing = [
            str(path.relative_to(root))
            for path in (header, implementation)
            if not path.is_file()
        ]

        if missing:
            self.fail(
                "RED_EXPECTED: VAM proximity provider ainda não implementado:\n"
                + "\n".join(missing)
            )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        with tempfile.TemporaryDirectory(
            prefix="vam-proximity-provider-"
        ) as temp:
            binary = Path(temp) / "test_vam_proximity_provider"

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
                "VAM_PROXIMITY_PROVIDER_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{build.stdout}\n"
                f"STDERR:\n{build.stderr}",
            )

            print("VAM_PROXIMITY_PROVIDER_BUILD=PASS")

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
            self.assertIn("VAM_PROXIMITY_PROVIDER=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
