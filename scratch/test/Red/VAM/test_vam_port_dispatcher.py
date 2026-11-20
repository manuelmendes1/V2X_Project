import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestVamPortDispatcher(unittest.TestCase):
    def test_vam_port_2018_is_isolated_from_other_btp_handlers(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++") or shutil.which("g++-13") or shutil.which("g++-11")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        source = (
            root / "scratch/test/Red/VAM/test_vam_port_dispatcher.cc"
        )

        vanetza_root = Path(
            os.environ.get("V2X_VANETZA_ROOT", "/usr/local")
        )
        lib_dir = vanetza_root / "lib"
        if not (lib_dir / "libvanetza_btp.so").exists():
            lib_dir = Path("/usr/local/lib")

        include_dirs = [
            vanetza_root / "include",
            vanetza_root / "include/vanetza",
            vanetza_root / "include/vanetza/asn1",
            vanetza_root / "include/vanetza/asn1/support",
        ]

        with tempfile.TemporaryDirectory(
            prefix="vam-port-dispatcher-"
        ) as temp:
            binary = Path(temp) / "test_vam_port_dispatcher"

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                *[f"-I{path}" for path in include_dirs],
                str(source),
                f"-L{lib_dir}",
                f"-Wl,-rpath,{lib_dir}",
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
                cwd=root,
                capture_output=True,
                text=True,
                timeout=60,
                check=False,
            )
            if build.returncode != 0:
                self.fail(
                    "VAM_PORT_DISPATCHER_BUILD=FAIL\n"
                    f"Command: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            env = os.environ.copy()
            env["LD_LIBRARY_PATH"] = (
                str(lib_dir) + ":" + env.get("LD_LIBRARY_PATH", "")
            )
            run = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                timeout=20,
                check=False,
                env=env,
            )

            output = run.stdout + run.stderr
            print("VAM_PORT_DISPATCHER_BUILD=PASS")
            print(output, end="")

            self.assertEqual(run.returncode, 0, msg=output)
            self.assertIn("VAM_BTP_PORT_DISPATCHER=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
