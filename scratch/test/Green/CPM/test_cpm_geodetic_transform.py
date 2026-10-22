#!/usr/bin/env python3

import subprocess
import sys
import unittest
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SCRATCH = ROOT / "scratch"
BASE = SCRATCH / "vanetza_integration/cpm/perception"
HEADER = BASE / "cpm_geodetic_transform.h"
SOURCE = BASE / "cpm_geodetic_transform.cc"
TEST = Path(__file__).with_suffix(".cc")
GEOLIB = Path.home() / "opt/geographiclib-gcc11"

def main() -> int:
    missing = [
        str(path.relative_to(ROOT))
        for path in (HEADER, SOURCE)
        if not path.is_file()
    ]
    if missing:
        print("RED_EXPECTED: implementação geodésica ainda ausente.")
        for path in missing:
            print(f"  - {path}")
        return 1

    include = GEOLIB / "include"
    lib = GEOLIB / "lib"

    if not (include / "GeographicLib/LocalCartesian.hpp").is_file():
        print(f"ERRO: header GeographicLib inexistente: {include}")
        return 2
    if not (lib / "libGeographicLib.so").exists():
        print(f"ERRO: biblioteca GeographicLib inexistente: {lib}")
        return 2

    with tempfile.TemporaryDirectory(prefix="cpm-geodetic-test-") as temp:
        binary = Path(temp) / "test_cpm_geodetic_transform"

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
            str(SOURCE),
            "-L", str(lib),
            f"-Wl,-rpath,{lib}",
            "-lGeographicLib",
            "-o", str(binary),
        ]

        build = subprocess.run(
            command, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, check=False
        )
        if build.stdout:
            print(build.stdout, end="")
        if build.returncode != 0:
            print(f"CPM_GEODETIC_TRANSFORM_BUILD=FAIL ({build.returncode})")
            return build.returncode

        print("CPM_GEODETIC_TRANSFORM_BUILD=PASS")
        result = subprocess.run(
            [str(binary)], text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, check=False
        )
        if result.stdout:
            print(result.stdout, end="")
        return result.returncode

class TestCpmGeodeticTransform(unittest.TestCase):
    def test_geodetic_transform_contract(self):
        self.assertEqual(main(), 0)


if __name__ == "__main__":
    sys.exit(main())
