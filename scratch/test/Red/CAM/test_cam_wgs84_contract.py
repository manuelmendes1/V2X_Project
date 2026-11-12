import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def find_repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (candidate / "scratch" / "CMakeLists.txt").is_file():
            return candidate
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestCamWgs84Contract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="cam-wgs84-contract-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_cam_wgs84_contract"

        compiler = shutil.which("g++-11") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado g++-11 nem g++.")

        integration = cls.root / "scratch/vanetza_integration"
        test_source = (
            cls.root / "scratch/test/Red/CAM/"
            "test_cam_wgs84_contract.cc"
        )

        sources = [
            integration / "cam/validation/cam_validator.cc",
            integration / "wrappers/vanetza/cam/vanetza_cam_codec.cc",
            integration / "wrappers/vanetza/cam/cam_asn1_builder.cc",
            integration / "wrappers/vanetza/cam/cam_converter.cc",
            integration / "wrappers/vanetza/cam/uper/cam_uper_encoder.cc",
            integration / "wrappers/vanetza/cam/uper/cam_uper_decoder.cc",
        ]

        missing = [str(source) for source in sources if not source.is_file()]
        if missing:
            raise RuntimeError(
                "Faltam fontes CAM necessárias:\n" + "\n".join(missing)
            )

        libraries = [
            "-lvanetza_access",
            "-lvanetza_asn1",
            "-lvanetza_asn1_its",
            "-lvanetza_asn1_security",
            "-lvanetza_asn1_support",
            "-lvanetza_btp",
            "-lvanetza_common",
            "-lvanetza_dcc",
            "-lvanetza_facilities",
            "-lvanetza_geonet",
            "-lvanetza_gnss",
            "-lvanetza_net",
            "-lvanetza_security",
            "-lssl",
            "-lcrypto",
        ]

        command = [
            compiler,
            "-std=c++23",
            "-O0",
            "-g",
            f"-I{cls.root / 'scratch'}",
            f"-I{integration}",
            "-I/usr/local/include",
            "-I/usr/local/include/vanetza",
            "-I/usr/local/include/vanetza/asn1",
            "-I/usr/local/include/vanetza/asn1/support",
            str(test_source),
            *(str(source) for source in sources),
            "-L/usr/local/lib",
            "-Wl,-rpath,/usr/local/lib",
            *libraries,
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
                "CAM_WGS84_COMPILE=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{compiled.stdout}\n"
                f"STDERR:\n{compiled.stderr}"
            )

        print(f"CAM_WGS84_COMPILE=PASS", flush=True)
        print(f"CAM_WGS84_ROOT={cls.root}", flush=True)

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_wgs84_contract(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

        output = result.stdout + result.stderr
        print(output, end="", flush=True)

        self.assertEqual(result.returncode, 0, msg=output)
        self.assertIn("CAM_WGS84_CONTRACT=PASS", output)
        self.assertIn("CAM_WGS84_CONTRACT_FAILED=0", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
