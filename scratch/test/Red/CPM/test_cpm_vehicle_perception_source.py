#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SCRATCH = ROOT / "scratch"
TEST = Path(__file__).with_suffix(".cc")
GEOLIB = Path.home() / "opt/geographiclib-gcc11"

IMPLEMENTATION = (
    SCRATCH / "vanetza_integration/cpm/perception/"
    "cpm_vehicle_perception_source.cc"
)
HEADER = (
    SCRATCH / "vanetza_integration/cpm/perception/"
    "cpm_vehicle_perception_source.h"
)

SOURCES = [
    SCRATCH / "vanetza_integration/cpm/perception/"
    "cpm_geodetic_transform.cc",
    SCRATCH / "vanetza_integration/cpm/perception/"
    "cpm_coordinate_transform.cc",
    SCRATCH / "vanetza_integration/cpm/perception/"
    "cpm_temporal_transform.cc",
    SCRATCH / "vanetza_integration/cpm/perception/"
    "cpm_track_history.cc",
    SCRATCH / "vanetza_integration/cpm/perception/"
    "cpm_synthetic_perception_source.cc",
]


def read_cmake_cache_value(cache_path, key):
    prefix = key + ":"

    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()

    raise RuntimeError(
        f"Variavel {key} nao encontrada em {cache_path}"
    )


def main() -> int:
    if not HEADER.is_file() or not IMPLEMENTATION.is_file():
        print("CPM_VEHICLE_PERCEPTION_SOURCE=RED_EXPECTED")
        print("Implementacao de producao ainda ausente:")
        for path in (HEADER, IMPLEMENTATION):
            if not path.is_file():
                print(f"  - {path.relative_to(ROOT)}")
        return 1

    cache_path = ROOT / "cmake-cache" / "CMakeCache.txt"
    if not cache_path.is_file():
        print(f"CMakeCache nao encontrado: {cache_path}")
        return 2

    compiler = Path(
        read_cmake_cache_value(cache_path, "CMAKE_CXX_COMPILER")
    )

    if not compiler.is_file():
        print(f"Compilador configurado nao encontrado: {compiler}")
        return 2

    include = GEOLIB / "include"
    lib = GEOLIB / "lib"

    required = [TEST, *SOURCES]
    missing = [path for path in required if not path.is_file()]

    if missing:
        for path in missing:
            print(f"Ficheiro necessario em falta: {path}")
        return 2

    if not include.is_dir() or not lib.is_dir():
        print(f"GeographicLib prefix incompleto: {GEOLIB}")
        return 2

    with tempfile.TemporaryDirectory(
        prefix="cpm-vehicle-perception-"
    ) as temp:
        binary = Path(temp) / "test_cpm_vehicle_perception_source"
        link_map = Path(temp) / "cpm-vehicle-perception-link.map"

        command = [
            str(compiler),
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            "-I", str(SCRATCH),
            "-I", str(include),
            str(TEST),
            str(IMPLEMENTATION),
            *(str(path) for path in SOURCES),
            "-L", str(lib),
            f"-Wl,-rpath,{lib}",
            f"-Wl,-Map={link_map}",
            "-lGeographicLib",
            "-o", str(binary),
        ]

        build = subprocess.run(
            command,
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
            timeout=120,
        )

        if build.stdout:
            print(build.stdout, end="")

        if build.returncode != 0:
            print(
                f"CPM_VEHICLE_PERCEPTION_BUILD=FAIL "
                f"({build.returncode})"
            )
            return build.returncode

        if not link_map.is_file():
            print("CPM_VEHICLE_PERCEPTION_LINK_MAP=FAIL")
            return 3

        map_content = link_map.read_text(
            encoding="utf-8", errors="replace"
        )

        if str(lib) not in map_content or "GeographicLib" not in map_content:
            print("CPM_VEHICLE_PERCEPTION_GEOLIB_LINK=FAIL")
            return 3

        print(f"CPM_VEHICLE_PERCEPTION_COMPILER={compiler}")
        print(f"CPM_VEHICLE_PERCEPTION_GEOLIB={GEOLIB}")
        print("CPM_VEHICLE_PERCEPTION_LINK_MAP=PASS")
        print("CPM_VEHICLE_PERCEPTION_BUILD=PASS")

        result = subprocess.run(
            [str(binary)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
            timeout=30,
        )

        if result.stdout:
            print(result.stdout, end="")

        return result.returncode


class TestCpmVehiclePerceptionSource(unittest.TestCase):

    def test_vehicle_perception_source_contract(self):
        self.assertEqual(main(), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
