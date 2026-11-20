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


class TestVamBtpTxBridge(unittest.TestCase):
    def test_real_btp_router_and_tx_bridge_handoff(self):
        root = repository_root(Path(__file__))
        compiler = shutil.which("g++-11")
        self.assertIsNotNone(compiler, "g++-11 é obrigatório")

        prefix = Path.home() / "opt/vanetza-cpm-gcc11"
        library_dir = prefix / "lib"
        geographiclib = (
            Path.home() / "opt/geographiclib-gcc11/lib/libGeographicLib.so"
        )
        source = root / "scratch/test/Red/VAM/test_vam_btp_tx_bridge.cc"
        adapter_object = (
            root / "cmake-cache/scratch/CMakeFiles/vanetza_cam_encoder_obj.dir/"
            "vanetza_integration/wrappers/vanetza/btp/vanetza_btp_adapter.cc.o"
        )
        bridge_source = (
            root / "scratch/vanetza_integration/context/tx_bridge/tx_bridge_context.cc"
        )
        completion_source = (
            root / "scratch/vanetza_integration/context/tx_bridge/tx_completion.cc"
        )
        outcome_source = (
            root / "scratch/vanetza_integration/context/tx_bridge/tx_binding_outcome.cc"
        )

        for required in (
            source,
            adapter_object,
            bridge_source,
            completion_source,
            outcome_source,
            library_dir / "libvanetza_geonet.a",
            geographiclib,
        ):
            self.assertTrue(required.is_file(), f"Ficheiro/dependência em falta: {required}")

        vanetza_libraries = [
            "-lvanetza_access", "-lvanetza_asn1", "-lvanetza_asn1_its",
            "-lvanetza_asn1_its_r2", "-lvanetza_asn1_security",
            "-lvanetza_asn1_support", "-lvanetza_btp", "-lvanetza_common",
            "-lvanetza_dcc", "-lvanetza_facilities", "-lvanetza_geonet",
            "-lvanetza_geodesy", "-lvanetza_gnss", "-lvanetza_net",
            "-lvanetza_security", "-lvanetza_asn1_security_r2",
        ]

        with tempfile.TemporaryDirectory(prefix="vam-btp-tx-bridge-") as temp:
            binary = Path(temp) / "test_vam_btp_tx_bridge"
            command = [
                compiler, "-std=c++23", "-O0", "-g", "-Wall", "-Wextra",
                "-Wpedantic", "-Werror", "-pthread",
                "-I", str(root / "build/include"),
                "-I", str(root / "scratch"),
                "-I", str(root / "scratch/vanetza_integration"),
                "-I", str(prefix / "include"),
                "-I", str(prefix / "include/vanetza"),
                "-I", str(prefix / "include/vanetza/asn1"),
                "-I", str(prefix / "include/vanetza/asn1/support"),
                str(source), str(bridge_source), str(completion_source),
                str(outcome_source), str(adapter_object),
                "-L", str(library_dir), "-L/usr/local/lib",
                f"-Wl,-rpath,{library_dir}:/usr/local/lib:{geographiclib.parent}",
                "-Wl,--start-group", *vanetza_libraries, "-Wl,--end-group",
                "-lcryptopp", str(geographiclib), "-lssl", "-lcrypto",
                "-pthread", "-o", str(binary),
            ]
            build = subprocess.run(
                command, cwd=root, capture_output=True, text=True, timeout=120
            )
            if build.returncode != 0:
                self.fail(
                    "VAM_BTP_TX_BRIDGE_BUILD=FAIL\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}"
                )

            env = os.environ.copy()
            env["LD_LIBRARY_PATH"] = (
                f"{library_dir}:{geographiclib.parent}:/usr/local/lib:"
                + env.get("LD_LIBRARY_PATH", "")
            )
            run = subprocess.run(
                [str(binary)], cwd=root, env=env,
                capture_output=True, text=True, timeout=30
            )
            output = run.stdout + run.stderr
            print("VAM_BTP_TX_BRIDGE_BUILD=PASS")
            print(output, end="")
            self.assertEqual(run.returncode, 0, f"TX bridge falhou, RC={run.returncode}")
            self.assertIn("VAM_BTP_TX_BRIDGE=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
