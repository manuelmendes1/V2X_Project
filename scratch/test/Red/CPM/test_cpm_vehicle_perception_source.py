#!/usr/bin/env python3

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SCRATCH = ROOT / "scratch"
BASE = SCRATCH / "vanetza_integration/cpm/perception"
TEST = Path(__file__).with_suffix(".cc")
GEOLIB = Path.home() / "opt/geographiclib-gcc11"

IMPLEMENTATION = BASE / "cpm_vehicle_perception_source.cc"
HEADER = BASE / "cpm_vehicle_perception_source.h"

SOURCES = [
    BASE / "cpm_geodetic_transform.cc",
    BASE / "cpm_coordinate_transform.cc",
    BASE / "cpm_temporal_transform.cc",
    BASE / "cpm_track_history.cc",
    BASE / "cpm_synthetic_perception_source.cc",
]


def main() -> int:
    if not HEADER.is_file() or not IMPLEMENTATION.is_file():
        print("CPM_VEHICLE_PERCEPTION_SOURCE=RED_EXPECTED")
        print("Implementação de produção ainda ausente:")
        for path in (HEADER, IMPLEMENTATION):
            if not path.is_file():
                print(f"  - {path.relative_to(ROOT)}")
        return 1

    include = GEOLIB / "include"
    lib = GEOLIB / "lib"

    required = [TEST, *SOURCES]
    missing = [path for path in required if not path.is_file()]
    if missing:
        for path in missing:
            print(f"Ficheiro necessário em falta: {path}")
        return 2

    with tempfile.TemporaryDirectory(
        prefix="cpm-vehicle-perception-"
    ) as temp:
        binary = Path(temp) / "test_cpm_vehicle_perception_source"

        command = [
            "g++",
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
            "-lGeographicLib",
            "-o", str(binary),
        ]

        build = subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )

        if build.stdout:
            print(build.stdout, end="")

        if build.returncode != 0:
            print(f"CPM_VEHICLE_PERCEPTION_BUILD=FAIL ({build.returncode})")
            return build.returncode

        print("CPM_VEHICLE_PERCEPTION_BUILD=PASS")

        result = subprocess.run(
            [str(binary)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )

        if result.stdout:
            print(result.stdout, end="")

        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
