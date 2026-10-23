from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (candidate / "ns3").is_file() and (
            candidate / "scratch" / "CMakeLists.txt"
        ).is_file():
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestCpmPortDispatcherIsolation(unittest.TestCase):
    def test_btp_b_ports_dispatch_to_only_the_registered_handler(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++") or shutil.which("g++-11")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        source = (
            root / "scratch/test/Green/CPM/"
            "test_cpm_port_dispatcher_isolation.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="cpm-port-dispatcher-"
        ) as temp:
            binary = Path(temp) / "test_cpm_port_dispatcher_isolation"

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-I/usr/local/include",
                "-I/usr/local/include/vanetza",
                "-I/usr/local/include/vanetza/asn1",
                "-I/usr/local/include/vanetza/asn1/support",
                str(source),
                "-L/usr/local/lib",
                "-Wl,-rpath,/usr/local/lib",
                "-lvanetza_btp",
                "-lvanetza_geonet",
                "-lvanetza_net",
                "-lvanetza_security",
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
                    "CPM_PORT_DISPATCHER_BUILD=FAIL\n"
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
            print("CPM_PORT_DISPATCHER_BUILD=PASS")
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                f"Dispatch isolation failed, RC={run.returncode}",
            )
            self.assertIn(
                "CPM_PORT_DISPATCHER_ISOLATION=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main()
