from pathlib import Path
import os
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
    raise RuntimeError("ns-3 repository root not found")


class TestItsStationIdentityAuthority(unittest.TestCase):

    def test_global_station_identity_reservation_contract(self):
        root = repository_root(Path(__file__))

        relative = Path(
            "scratch/vanetza_integration/context/"
            "its_station_identity/its_station_identity_authority"
        )

        header = root / relative.with_suffix(".h")
        implementation = root / relative.with_suffix(".cc")

        missing = [
            str(path.relative_to(root))
            for path in (header, implementation)
            if not path.is_file()
        ]

        self.assertFalse(
            missing,
            "ITS_STATION_ID_AUTHORITY_IMPLEMENTATION_MISSING: "
            + ", ".join(missing),
        )

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "C++ compiler not found")

        test_source = (
            root / "scratch/test/Red/ITSStation/"
            "test_its_station_identity_authority.cc"
        )

        with tempfile.TemporaryDirectory(
            prefix="its-station-identity-authority-"
        ) as temporary:
            binary = Path(temporary) / "test_its_station_identity_authority"

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                "-I",
                str(root / "scratch"),
                str(test_source),
                str(implementation),
                "-o",
                str(binary),
            ]

            compile_result = subprocess.run(
                command,
                cwd=root,
                text=True,
                capture_output=True,
                timeout=120,
            )

            self.assertEqual(
                compile_result.returncode,
                0,
                "ITS_STATION_ID_AUTHORITY_COMPILE=FAIL\n"
                f"STDOUT:\n{compile_result.stdout}\n"
                f"STDERR:\n{compile_result.stderr}",
            )

            env = os.environ.copy()
            run = subprocess.run(
                [str(binary)],
                cwd=root,
                env=env,
                text=True,
                capture_output=True,
                timeout=15,
            )

            output = run.stdout + run.stderr
            print(output)

            self.assertEqual(
                run.returncode,
                0,
                "ITS_STATION_ID_AUTHORITY_RUNTIME=FAIL",
            )

            self.assertIn(
                "ITS_STATION_ID_AUTHORITY=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
