import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def find_repository_root(start):
    resolved = start.resolve()

    for candidate in (resolved, *resolved.parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate

    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestSpatemReplayProtection(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="spatem-replay-protection-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_spatem_replay_protection"

        compiler = shutil.which("g++-11") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado g++-11 nem g++.")

        integration = cls.root / "scratch/vanetza_integration"
        test_source = (
            cls.root / "scratch/test/Red/SPATEM/"
            "test_spatem_replay_protection.cc"
        )

        sources = [
            integration / "spatem/replay/spatem_replay_protection.cc",
            integration / "context/packet/packet_context.cc",
            integration / "context/packet/packet_payload.cc",
            integration / "context/packet/packet_types.cc",
        ]

        command = [
            compiler,
            "-std=c++23",
            "-O0",
            "-g",
            f"-I{cls.root / 'scratch'}",
            f"-I{cls.root / 'build/include'}",
            f"-I{integration}",
            f"-I{cls.root / 'src'}",
            str(test_source),
            *(str(source) for source in sources),
            "-o",
            str(cls.binary),
        ]

        compiled = subprocess.run(
            command,
            cwd=cls.root,
            capture_output=True,
            text=True,
            check=False,
        )

        if compiled.returncode != 0:
            raise RuntimeError(
                "Compilação do teste C++ falhou.\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{compiled.stdout}\n"
                f"STDERR:\n{compiled.stderr}"
            )

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_replay_protection_contract(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

        print(result.stdout, end="")
        self.assertEqual(
            result.returncode,
            0,
            msg=f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}",
        )
        self.assertIn("SPATEM_REPLAY_TESTS=PASS", result.stdout)
        self.assertIn("SPATEM_REPLAY_PASSED=6", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
