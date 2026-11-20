from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate

    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestVamTransportGateway(unittest.TestCase):
    def test_gateway_preserves_payload_context_and_vam_policy(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        source = (
            root / "scratch/test/Red/VAM/"
            "test_vam_transport_gateway.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="vam-transport-gateway-"
        ) as temp:
            binary = Path(temp) / "test_vam_transport_gateway"

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                f"-I{root / 'build/include'}",
                f"-I{root / 'cmake-cache/include'}",
                "-I/usr/local/include",
                "-I/usr/local/include/vanetza",
                "-I/usr/local/include/vanetza/asn1",
                "-I/usr/local/include/vanetza/asn1/support",
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
            )

            if build.returncode != 0:
                self.fail(
                    "VAM_TRANSPORT_GATEWAY_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            run = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                timeout=20,
            )

            output = run.stdout + run.stderr
            print("VAM_TRANSPORT_GATEWAY_BUILD=PASS")
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                f"Gateway VAM falhou, RC={run.returncode}",
            )
            self.assertIn("VAM_TRANSPORT_GATEWAY=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
