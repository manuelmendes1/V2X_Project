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


class TestCpmR2PerceivedObjectProbe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="cpm-r2-perceived-object-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_cpm_r2_perceived_object"

        compiler = shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado o compilador g++.")

        source = (
            cls.root
            / "scratch/test/Green/CPM/"
            "test_cpm_r2_perceived_object_probe.cc"
        )

        command = [
            compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            f"-I{cls.root / 'scratch'}",
            f"-I{cls.root / 'build/include'}",
            "-I/usr/local/include",
            "-I/usr/local/include/vanetza/asn1/support",
            str(source),
            "-L/usr/local/lib",
            "-Wl,-rpath,/usr/local/lib",
            "-lvanetza_asn1_its_r2",
            "-lvanetza_asn1",
            "-lvanetza_asn1_support",
            "-o",
            str(cls.binary),
        ]

        result = subprocess.run(
            command,
            cwd=cls.root,
            capture_output=True,
            text=True,
            check=False,
        )

        if result.returncode != 0:
            raise RuntimeError(
                "Compilação do probe CPM R2 falhou.\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        print("CPM_OBJECT_RUNNER_BUILD=PASS")

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_roundtrip_and_semantic_negative_cases(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

        output = result.stdout + result.stderr
        print(output, end="")

        self.assertEqual(result.returncode, 0, msg=output)

        expected = [
            "CPM_OBJECT_ASN1_CONSTRAINTS=PASS",
            "CPM_OBJECT_UPER_ENCODE=PASS",
            "CPM_OBJECT_VALID_ROUNDTRIP=PASS",
            "CPM_OBJECT_ORIGIN_VEHICLE_ANGLE=0",
            "CPM_OBJECT_ORIGIN_VEHICLE_CONFIDENCE=127",
            "CPM_OBJECT_ORIGIN_CONTAINER_ID=1",
            "CPM_EMPTY_PAYLOAD_REJECTED=PASS",
            "CPM_TRUNCATED_PAYLOAD_REJECTED=PASS",
            "CPM_MISSING_OBJECT_ID_ROOT_CONSTRAINT_DID_NOT_REJECT=CONFIRMED",
            "CPM_MISSING_OBJECT_ID_LIST_CONSTRAINT_DID_NOT_REJECT=CONFIRMED",
            "CPM_MISSING_OBJECT_ID_REJECTED=PASS",
            "CPM_COUNT_TOTAL_GREATER_THAN_INCLUDED=PASS",
            "CPM_COUNT_BELOW_INCLUDED_ASN_CONSTRAINT_RC=",
            "CPM_COUNT_BELOW_INCLUDED_REJECTED=PASS",
            "CPM_R2_PERCEIVED_OBJECT_PROBE=PASS",
            "CPM_SEMANTIC_BOUNDARY_MATRIX=PASS",
            "CPM_BOUNDARY_OBJECT_ID_MIN=PASS",
            "CPM_BOUNDARY_OBJECT_ID_MAX=PASS",
            "CPM_BOUNDARY_X_COORDINATE_MIN=PASS",
            "CPM_BOUNDARY_X_COORDINATE_MAX=PASS",
            "CPM_BOUNDARY_Y_COORDINATE_MIN=PASS",
            "CPM_BOUNDARY_Y_COORDINATE_MAX=PASS",
            "CPM_BOUNDARY_CONFIDENCE_MIN=PASS",
            "CPM_BOUNDARY_CONFIDENCE_MAX=PASS",
            "CPM_BOUNDARY_MEASUREMENT_DELTA_MIN=PASS",
            "CPM_BOUNDARY_MEASUREMENT_DELTA_MAX=PASS",
            "CPM_BOUNDARY_QUALITY_MIN=PASS",
            "CPM_BOUNDARY_QUALITY_MAX=PASS",
            "CPM_BOUNDARY_OBJECT_ID_BELOW_MIN=PASS",
            "CPM_BOUNDARY_OBJECT_ID_ABOVE_MAX=PASS",
            "CPM_BOUNDARY_X_COORDINATE_BELOW_MIN=PASS",
            "CPM_BOUNDARY_X_COORDINATE_ABOVE_MAX=PASS",
            "CPM_BOUNDARY_Y_COORDINATE_BELOW_MIN=PASS",
            "CPM_BOUNDARY_Y_COORDINATE_ABOVE_MAX=PASS",
            "CPM_BOUNDARY_CONFIDENCE_BELOW_MIN=PASS",
            "CPM_BOUNDARY_CONFIDENCE_ABOVE_MAX=PASS",
            "CPM_BOUNDARY_MEASUREMENT_DELTA_BELOW_MIN=PASS",
            "CPM_BOUNDARY_MEASUREMENT_DELTA_ABOVE_MAX=PASS",
            "CPM_BOUNDARY_QUALITY_BELOW_MIN=PASS",
            "CPM_BOUNDARY_QUALITY_ABOVE_MAX=PASS",
        ]

        for marker in expected:
            with self.subTest(marker=marker):
                self.assertIn(marker, output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
