from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


def repository_root(path: Path) -> Path:
    for candidate in path.resolve().parents:
        if (candidate / "ns3").is_file() and (candidate / "scratch").is_dir():
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestCpmVanetzaCopyRequestParameters(unittest.TestCase):
    def test_optional_limits_are_copied_to_geonetworking_request(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++") or shutil.which("g++-11")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        source = (
            root / "scratch/test/Green/CPM/"
            "test_cpm_vanetza_copy_request_parameters.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="cpm-vanetza-copy-request-"
        ) as temp:
            binary = Path(temp) / "test_cpm_vanetza_copy_request_parameters"

            command = [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-I/usr/local/include",
                str(source),
                "-L/usr/local/lib",
                "-Wl,-rpath,/usr/local/lib",
                "-lvanetza_geonet",
                "-lvanetza_btp",
                "-lvanetza_common",
                "-o",
                str(binary),
            ]

            build = subprocess.run(
                command,
                text=True,
                capture_output=True,
                timeout=60,
            )

            if build.returncode != 0:
                self.fail(
                    "CPM_VANETZA_COPY_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            env = os.environ.copy()
            env["LD_LIBRARY_PATH"] = (
                "/usr/local/lib:" + env.get("LD_LIBRARY_PATH", "")
            )

            run = subprocess.run(
                [str(binary)],
                text=True,
                capture_output=True,
                env=env,
                timeout=20,
            )

            output = run.stdout + run.stderr
            print("CPM_VANETZA_COPY_BUILD=PASS")
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                f"Teste de integração falhou, RC={run.returncode}",
            )
            self.assertIn(
                "CPM_VANETZA_COPY_REQUEST_PARAMETERS=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main()
