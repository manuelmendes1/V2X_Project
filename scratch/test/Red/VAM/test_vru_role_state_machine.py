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


class TestVruRoleStateMachine(unittest.TestCase):
    def test_role_and_vbs_transitions_preserve_state_invariants(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ não encontrado")

        sources = [
            root / "scratch/test/Red/VAM/test_vru_role_state_machine.cc",
            root / "scratch/vanetza_integration/vru/role/state_machine/vru_role_state_machine.cc",
        ]

        with tempfile.TemporaryDirectory(prefix="vru-role-state-machine-") as temp:
            binary = Path(temp) / "test_vru_role_state_machine"
            command = [
                compiler, "-std=c++17",
                "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                f"-I{root / 'scratch'}",
                *[str(source) for source in sources],
                "-o", str(binary),
            ]

            build = subprocess.run(
                command, cwd=root, capture_output=True,
                text=True, timeout=60,
            )

            if build.returncode != 0:
                self.fail(
                    "VRU_ROLE_STATE_MACHINE_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
                )

            run = subprocess.run(
                [str(binary)], cwd=root,
                capture_output=True, text=True, timeout=20,
            )
            output = run.stdout + run.stderr
            print("VRU_ROLE_STATE_MACHINE_BUILD=PASS")
            print(output, end="")

            self.assertEqual(run.returncode, 0, output)
            self.assertIn("VRU_ROLE_STATE_MACHINE=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
