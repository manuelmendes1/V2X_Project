import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def find_repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestSpatemRxSemanticTiming(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="spatem-rx-semantic-timing-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_spatem_rx_semantic_timing"

        compiler = shutil.which("g++-11") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado g++-11 nem g++.")

        integration = cls.root / "scratch/vanetza_integration"
        test_source = (
            cls.root / "scratch/test/Red/SPATEM/"
            "test_spatem_rx_semantic_timing.cc"
        )

        sources = [
            integration / "spatem/rx/spatem_rx_pipeline.cc",
            integration / "spatem/replay/spatem_replay_protection.cc",
            integration / "wrappers/vanetza/spatem/vanetza_spatem_codec.cc",
            integration / "wrappers/vanetza/spatem/spatem_asn1_builder.cc",
            integration / "wrappers/vanetza/spatem/spatem_converter.cc",
            integration / "wrappers/vanetza/spatem/uper/spatem_uper_encoder.cc",
            integration / "wrappers/vanetza/spatem/uper/spatem_uper_decoder.cc",
            integration / "context/packet/packet_context.cc",
            integration / "context/packet/packet_payload.cc",
            integration / "context/packet/packet_types.cc",
        ]

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
            f"-I{cls.root / 'build/include'}",
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
                "Compilação do teste C++ falhou.\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{compiled.stdout}\n"
                f"STDERR:\n{compiled.stderr}"
            )

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_rx_rejects_inconsistent_timing_before_replay(self):
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
        self.assertIn("SPATEM_RX_SEMANTIC_TIMING_TESTS=PASS", output)
        self.assertIn("SPATEM_RX_SEMANTIC_TIMING_FAILED=0", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
