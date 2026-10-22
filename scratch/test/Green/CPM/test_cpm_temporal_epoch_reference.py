import shutil
import subprocess
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path


def repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestCpmTemporalEpochReference(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        compiler = shutil.which("g++")
        if not compiler:
            raise RuntimeError("Compilador g++ não encontrado.")

        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="cpm-temporal-epoch-reference-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_temporal_epoch"

        test_source = (
            cls.root / "scratch/test/Green/CPM/"
            "test_cpm_temporal_epoch_reference.cc"
        )
        implementation = (
            cls.root / "scratch/vanetza_integration/cpm/perception/"
            "cpm_temporal_transform.cc"
        )

        command = [
            compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{cls.root / 'scratch'}",
            str(test_source),
            str(implementation),
            "-o",
            str(cls.binary),
        ]
        result = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        if result.returncode != 0:
            cls.tempdir.cleanup()
            raise RuntimeError(
                "Falha a compilar o teste temporal:\n"
                + result.stdout + result.stderr
            )

        print("CPM_TEMPORAL_EPOCH_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_known_utc_epoch_anchor(self):
        its_epoch = datetime(2004, 1, 1, tzinfo=timezone.utc)
        known_utc = datetime(2026, 10, 22, 10, 0, 0,
                             tzinfo=timezone.utc)
        delta = known_utc - its_epoch
        independently_calculated_us = (
            (delta.days * 86400 + delta.seconds) * 1_000_000
            + delta.microseconds
        )

        self.assertEqual(independently_calculated_us, 719748000000000)

    def test_transform_matches_known_elapsed_times(self):
        result = subprocess.run(
            [str(self.binary)],
            capture_output=True,
            text=True,
            check=False,
        )
        output = result.stdout + result.stderr
        print(output, end="")

        self.assertEqual(result.returncode, 0, msg=output)
        self.assertIn("CPM_TEMPORAL_EPOCH_REFERENCE=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
