#!/usr/bin/env python3

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SCRATCH = ROOT / "scratch"
INTEGRATION = SCRATCH / "vanetza_integration"
TEST_SOURCE = Path(__file__).with_suffix(".cc")

CACHE_CANDIDATES = (
    ROOT / "cmake-cache-c02" / "CMakeCache.txt",
    ROOT / "cmake-cache" / "CMakeCache.txt",
    ROOT / "build-c02" / "CMakeCache.txt",
    ROOT / "build" / "CMakeCache.txt",
)

VANETZA_LIBRARIES = (
    "-lvanetza_access",
    "-lvanetza_asn1",
    "-lvanetza_asn1_its",
    "-lvanetza_asn1_its_r2",
    "-lvanetza_asn1_security",
    "-lvanetza_asn1_security_r2",
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
)

SOURCES = (
    INTEGRATION / "rx/rx_decode/rx_decode.cc",
    INTEGRATION / "security/replay/replay_protection.cc",
    INTEGRATION / "cam/decoder/cam_decoder.cc",
    INTEGRATION / "wrappers/vanetza/cam/vanetza_cam_codec.cc",
    INTEGRATION / "wrappers/vanetza/cam/cam_asn1_builder.cc",
    INTEGRATION / "wrappers/vanetza/cam/cam_converter.cc",
    INTEGRATION / "wrappers/vanetza/cam/uper/cam_uper_encoder.cc",
    INTEGRATION / "wrappers/vanetza/cam/uper/cam_uper_decoder.cc",
    INTEGRATION / "context/packet/packet_context.cc",
    INTEGRATION / "context/packet/packet_payload.cc",
    INTEGRATION / "context/packet/packet_types.cc",
)


def parse_cmake_cache(path):
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        left, value = line.split("=", 1)
        key = left.split(":", 1)[0]
        values[key] = value.strip()
    return values


def discover_build_configuration():
    for cache_path in CACHE_CANDIDATES:
        if not cache_path.is_file():
            continue
        values = parse_cmake_cache(cache_path)
        vanetza_root = values.get("V2X_VANETZA_ROOT")
        if vanetza_root and (Path(vanetza_root) / "lib").is_dir():
            return cache_path, values, Path(vanetza_root).resolve()

    configured = os.environ.get("V2X_VANETZA_ROOT")
    if configured and (Path(configured) / "lib").is_dir():
        return None, {}, Path(configured).resolve()

    raise RuntimeError(
        "Não foi possível localizar V2X_VANETZA_ROOT num CMakeCache válido. "
        "Caches verificados: "
        + ", ".join(str(path) for path in CACHE_CANDIDATES)
    )


def first_existing_include(candidates, required_header):
    for candidate in candidates:
        if (candidate / required_header).is_file():
            return candidate
    raise RuntimeError(
        f"Não foi encontrado {required_header}. Diretórios verificados: "
        + ", ".join(str(path) for path in candidates)
    )


class TestCamRxIdentityBinding(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cache_path, cache_values, cls.vanetza_root = (
            discover_build_configuration()
        )

        configured_compiler = cache_values.get("CMAKE_CXX_COMPILER")
        compiler = None
        if configured_compiler:
            configured_path = Path(configured_compiler)
            if configured_path.is_file():
                compiler = str(configured_path)
            else:
                compiler = shutil.which(configured_compiler)

        compiler = compiler or shutil.which("g++-11") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado CMAKE_CXX_COMPILER, g++-11 ou g++.")

        missing = [str(path) for path in (TEST_SOURCE, *SOURCES) if not path.is_file()]
        if missing:
            raise RuntimeError(
                "Faltam fontes necessárias para o teste CAM RX:\n"
                + "\n".join(missing)
            )

        generated_include = first_existing_include(
            (
                ROOT / "cmake-cache-c02" / "include",
                ROOT / "cmake-cache" / "include",
                ROOT / "build-c02" / "include",
                ROOT / "build" / "include",
            ),
            "ns3/mac48-address.h",
        )

        include_dirs = (
            SCRATCH,
            INTEGRATION,
            generated_include,
            cls.vanetza_root / "include",
            cls.vanetza_root / "include/vanetza",
            cls.vanetza_root / "include/vanetza/asn1",
            cls.vanetza_root / "include/vanetza/asn1/support",
        )
        missing_include_dirs = [str(path) for path in include_dirs if not path.is_dir()]
        if missing_include_dirs:
            raise RuntimeError(
                "Diretórios include necessários em falta:\n"
                + "\n".join(missing_include_dirs)
            )

        lib_dirs = [cls.vanetza_root / "lib"]
        if (cls.vanetza_root / "lib64").is_dir():
            lib_dirs.append(cls.vanetza_root / "lib64")

        ns3_lib_dirs = (
            ROOT / "cmake-cache-c02" / "lib",
            ROOT / "cmake-cache" / "lib",
            ROOT / "build-c02" / "lib",
            ROOT / "build" / "lib",
        )
        for candidate in ns3_lib_dirs:
            if candidate.is_dir():
                lib_dirs.append(candidate)

        with tempfile.TemporaryDirectory(prefix="cam-rx-identity-binding-") as temp:
            cls.tempdir = temp
            cls.binary = Path(temp) / "test_cam_rx_identity_binding"

            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-O0",
                "-g",
            ]
            command.extend(f"-I{path}" for path in include_dirs)
            command.extend(str(path) for path in (TEST_SOURCE, *SOURCES))

            for lib_dir in lib_dirs:
                command.append(f"-L{lib_dir}")
                command.append(f"-Wl,-rpath,{lib_dir}")

            command.extend(("-Wl,--start-group", *VANETZA_LIBRARIES, "-Wl,--end-group"))
            command.extend(("-lcryptopp", "-lssl", "-lcrypto"))

            # PacketContext incorpora Mac48Address. Quando a árvore de build
            # expõe as bibliotecas ns-3, ligamo-las na ordem network -> core.
            available_ns3 = set()
            for lib_dir in lib_dirs:
                for library in ("ns3.47-network-default", "ns3.47-core-default"):
                    if any(
                        (lib_dir / f"lib{library}{suffix}").is_file()
                        for suffix in (".so", ".a", ".dylib")
                    ):
                        available_ns3.add(library)
            for library in ("ns3.47-network-default", "ns3.47-core-default"):
                if library in available_ns3:
                    command.append(f"-l{library}")

            command.extend(("-o", str(cls.binary)))

            compiled = subprocess.run(
                command,
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
                timeout=120,
            )

            if compiled.returncode != 0:
                raise RuntimeError(
                    "CAM_RX_IDENTITY_BINDING_COMPILE=FAIL\n"
                    f"CMakeCache: {cls.cache_path}\n"
                    f"Vanetza: {cls.vanetza_root}\n"
                    f"Comando: {' '.join(command)}\n"
                    f"STDOUT:\n{compiled.stdout}\n"
                    f"STDERR:\n{compiled.stderr}"
                )

            print("CAM_RX_IDENTITY_BINDING_COMPILE=PASS", flush=True)
            print(f"CAM_RX_IDENTITY_BINDING_COMPILER={compiler}", flush=True)
            print(f"CAM_RX_IDENTITY_BINDING_VANETZA={cls.vanetza_root}", flush=True)

            executed = subprocess.run(
                [str(cls.binary)],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
                timeout=30,
            )
            cls.output = executed.stdout + executed.stderr
            cls.harness_returncode = executed.returncode
            print(cls.output, end="", flush=True)

            if executed.returncode != 0:
                raise RuntimeError(
                    "O harness terminou com erro de execução, não com RED funcional.\n"
                    f"exit={executed.returncode}\n{cls.output}"
                )

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tempdir"):
            shutil.rmtree(cls.tempdir, ignore_errors=True)

    def assert_marker(self, marker):
        self.assertIn(f"PASS {marker}", self.output, msg=self.output)

    def test_mismatched_cam_station_id_is_rejected_at_decode(self):
        self.assert_marker("CAM_MISMATCH_REJECTED_AT_DECODE")

    def test_mismatched_cam_station_id_cannot_mutate_victim_replay_state(self):
        self.assert_marker("CAM_MISMATCH_BLOCKED_BEFORE_REPLAY")

    def test_packet_context_sender_station_id_remains_authoritative(self):
        self.assert_marker("CAM_MISMATCH_PRESERVED_CONTEXT_IDENTITY")

    def test_missing_packet_context_sender_identity_fails_closed(self):
        self.assert_marker("CAM_MISSING_SENDER_ID_FAIL_CLOSED")

    def test_matching_station_id_reaches_replay_and_updates_own_entry(self):
        self.assert_marker("CAM_MATCH_REACHES_REPLAY_AND_UPDATES_OWN_ENTRY")

    def test_zero_cam_station_id_remains_invalid(self):
        self.assert_marker("CAM_ZERO_STATION_ID_REMAINS_INVALID")


if __name__ == "__main__":
    unittest.main(verbosity=2)
