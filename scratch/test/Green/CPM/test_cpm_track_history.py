#!/usr/bin/env python3

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


def run_contract(root, test_source):
    base = root / "scratch/vanetza_integration/cpm/perception"
    header = base / "cpm_track_history.h"
    implementation = base / "cpm_track_history.cc"

    missing = [
        str(path.relative_to(root))
        for path in (header, implementation)
        if not path.is_file()
    ]

    if missing:
        print("RED_EXPECTED: histórico persistente ainda não implementado.")
        for item in missing:
            print(f"  - {item}")
        return 1

    compiler = shutil.which("g++")
    if not compiler:
        print("ERRO: não foi encontrado o compilador g++.")
        return 2

    with tempfile.TemporaryDirectory(
        prefix="cpm-track-history-"
    ) as temp:
        binary = Path(temp) / "test_cpm_track_history"

        perception = root / "scratch/vanetza_integration/cpm/perception"

        command = [
            compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{root / 'scratch'}",
            str(test_source),
            str(implementation),
            str(perception / "cpm_coordinate_transform.cc"),
            str(perception / "cpm_temporal_transform.cc"),
            str(perception / "cpm_synthetic_perception_source.cc"),
            "-o",
            str(binary),
        ]

        build = subprocess.run(
            command,
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )

        if build.stdout:
            print(build.stdout, end="")
        if build.stderr:
            print(build.stderr, end="")

        if build.returncode != 0:
            print(f"CPM_TRACK_HISTORY_BUILD=FAIL ({build.returncode})")
            return build.returncode

        print("CPM_TRACK_HISTORY_BUILD=PASS")

        result = subprocess.run(
            [str(binary)],
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )

        if result.stdout:
            print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, end="")

        return result.returncode


class TestCpmTrackHistory(unittest.TestCase):
    def test_persistent_track_history_contract(self):
        root = find_repository_root(Path(__file__))
        test_source = (
            root
            / "scratch/test/Green/CPM/test_cpm_track_history.cc"
        )

        self.assertEqual(
            run_contract(root, test_source),
            0,
            msg="O contrato do histórico persistente CPM falhou.",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
