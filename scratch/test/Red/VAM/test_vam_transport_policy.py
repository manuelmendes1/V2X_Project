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


class TestVamTransportPolicy(unittest.TestCase):
    def test_vam_btp_request_contract(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        source = (
            root / "scratch/test/Red/VAM/"
            "test_vam_transport_policy.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="vam-transport-policy-"
        ) as temp:
            binary = Path(temp) / "test_vam_transport_policy"

            command = [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                str(source),
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

            if build.returncode != 0:
                self.fail(
                    "VAM_TRANSPORT_POLICY_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            print("VAM_TRANSPORT_POLICY_BUILD=PASS")

            result = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
                timeout=20,
            )

            output = result.stdout + result.stderr
            print(output, end="")

            self.assertEqual(result.returncode, 0, msg=output)
            self.assertIn(
                "VAM_TRANSPORT_POLICY_TESTS=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
