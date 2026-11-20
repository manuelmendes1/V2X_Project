from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (candidate / "ns3").is_file() and (
            candidate / "scratch" / "CMakeLists.txt"
        ).is_file():
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestVamBtpAdapterTxCapture(unittest.TestCase):
    def test_real_adapter_routes_vam_btp_packet_to_dcc(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++-11")
        self.assertIsNotNone(
            compiler,
            "g++-11 é obrigatório para reutilizar o objeto de produção",
        )

        prefix = Path.home() / "opt/vanetza-cpm-gcc11"
        library_dir = prefix / "lib"
        geographiclib = (
            Path.home()
            / "opt/geographiclib-gcc11/lib/libGeographicLib.so"
        )

        source = (
            root / "scratch/test/Red/VAM/"
            "test_vam_btp_adapter_tx_capture.cc"
        )

        adapter_object = (
            root / "cmake-cache/scratch/CMakeFiles/"
            "vanetza_cam_encoder_obj.dir/"
            "vanetza_integration/wrappers/vanetza/btp/"
            "vanetza_btp_adapter.cc.o"
        )

        self.assertTrue(source.is_file(), f"Fonte em falta: {source}")
        self.assertTrue(
            adapter_object.is_file(),
            f"Objeto de produção em falta: {adapter_object}",
        )
        self.assertTrue(
            (library_dir / "libvanetza_geonet.a").is_file(),
            f"Biblioteca Vanetza em falta: {library_dir}",
        )
        self.assertTrue(
            geographiclib.is_file(),
            f"GeographicLib GCC11 em falta: {geographiclib}",
        )

        vanetza_libraries = [
            "-lvanetza_access",
            "-lvanetza_asn1",
            "-lvanetza_asn1_its",
            "-lvanetza_asn1_its_r2",
            "-lvanetza_asn1_security",
            "-lvanetza_asn1_support",
            "-lvanetza_btp",
            "-lvanetza_common",
            "-lvanetza_dcc",
            "-lvanetza_facilities",
            "-lvanetza_geonet",
            "-lvanetza_geodesy",
            "-lvanetza_gnss",
            "-lvanetza_net",
            "-lvanetza_security",
            "-lvanetza_asn1_security_r2",
        ]

        with tempfile.TemporaryDirectory(
            prefix="vam-btp-adapter-tx-"
        ) as temp:
            binary = Path(temp) / "test_vam_btp_adapter_tx_capture"

            command = [
                compiler,
                "-std=c++23",
                "-O0",
                "-g",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-I",
                str(root / "build/include"),
                "-I",
                str(root / "scratch"),
                "-I",
                str(root / "scratch/vanetza_integration"),
                "-I",
                str(prefix / "include"),
                "-I",
                str(prefix / "include/vanetza"),
                "-I",
                str(prefix / "include/vanetza/asn1"),
                "-I",
                str(prefix / "include/vanetza/asn1/support"),
                str(source),
                str(adapter_object),
                "-L",
                str(library_dir),
                "-L/usr/local/lib",
                f"-Wl,-rpath,{library_dir}:/usr/local/lib:"
                f"{geographiclib.parent}",
                "-Wl,--start-group",
                *vanetza_libraries,
                "-Wl,--end-group",
                "-lcryptopp",
                str(geographiclib),
                "-lssl",
                "-lcrypto",
                "-pthread",
                "-o",
                str(binary),
            ]

            build = subprocess.run(
                command,
                text=True,
                capture_output=True,
                timeout=120,
            )

            if build.returncode != 0:
                self.fail(
                    "VAM_BTP_ADAPTER_TX_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\n"
                    f"STDERR:\n{build.stderr}"
                )

            env = os.environ.copy()
            env["LD_LIBRARY_PATH"] = (
                f"{library_dir}:{geographiclib.parent}:"
                "/usr/local/lib:"
                + env.get("LD_LIBRARY_PATH", "")
            )

            run = subprocess.run(
                [str(binary)],
                text=True,
                capture_output=True,
                env=env,
                timeout=30,
            )

            output = run.stdout + run.stderr

            print("VAM_BTP_ADAPTER_TX_BUILD=PASS")
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                f"TX capture falhou, RC={run.returncode}",
            )
            self.assertIn(
                "VAM_BTP_ADAPTER_TX_CAPTURE=PASS",
                output,
            )


if __name__ == "__main__":
    unittest.main()
