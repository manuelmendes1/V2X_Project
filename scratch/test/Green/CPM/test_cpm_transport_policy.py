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


class TestCpmTransportPolicy(unittest.TestCase):
    def test_cpm_btp_request_contract(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        test_source = (
            root / "scratch/test/Green/CPM/"
            "test_cpm_transport_policy.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="cpm-transport-policy-"
        ) as temp:
            binary = Path(temp) / "test_cpm_transport_policy"

            command = [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                f"-I{root / 'scratch'}",
                str(test_source),
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

            if build.returncode != 0:
                self.fail(
                    "CPM_TRANSPORT_POLICY_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            print("CPM_TRANSPORT_POLICY_BUILD=PASS")

            result = subprocess.run(
                [str(binary)],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
            )

            output = result.stdout + result.stderr
            print(output, end="")

            self.assertEqual(result.returncode, 0, msg=output)
            self.assertIn(
                "CPM_TRANSPORT_POLICY_TESTS=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
