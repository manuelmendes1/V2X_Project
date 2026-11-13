#!/usr/bin/env python3

import subprocess
import unittest
from pathlib import Path


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "scratch" / "test" / "run_tests.py").is_file()
            and (candidate / "scratch" / "vanetza_integration").is_dir()
        ):
            return candidate

    raise RuntimeError("NS3_REPOSITORY_ROOT_NOT_FOUND")


class TestVamGenerationManagerProductionBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.cmake = (
            cls.root / "scratch" / "CMakeLists.txt"
        ).read_text(encoding="utf-8")

        configure = subprocess.run(
            ["cmake", "-S", ".", "-B", "cmake-cache"],
            cwd=cls.root,
            capture_output=True,
            text=True,
            timeout=120,
        )
        if configure.returncode != 0:
            raise RuntimeError(
                "VAM_PRODUCTION_CMAKE_RECONFIGURE_FAILED\n"
                f"STDOUT:\n{configure.stdout}\n"
                f"STDERR:\n{configure.stderr}"
            )

        result = subprocess.run(
            [
                "ninja", "-C", "cmake-cache",
                "-t", "commands",
                "ns3.47-v2x_sumo-default",
            ],
            cwd=cls.root,
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode != 0:
            raise RuntimeError(
                "NINJA_PRODUCTION_COMMAND_QUERY_FAILED\n"
                f"STDOUT:\n{result.stdout}\n"
                f"STDERR:\n{result.stderr}"
            )

        cls.commands = result.stdout.splitlines()

    def test_manager_is_registered_once_in_vam_object_library(self):
        start = self.cmake.find("add_library(vanetza_vam_obj OBJECT")
        self.assertGreaterEqual(
            start, 0, "VAM_OBJECT_LIBRARY_DECLARATION_MISSING"
        )

        end = self.cmake.find(")", start)
        self.assertGreater(end, start)

        library_sources = self.cmake[start:end]

        self.assertEqual(
            library_sources.count("vam_generation_manager.cc"),
            1,
            "VAM_GENERATION_MANAGER_SOURCE_NOT_REGISTERED_EXACTLY_ONCE",
        )
        self.assertEqual(
            library_sources.count("vam_proximity_provider.cc"),
            1,
            "VAM_PROXIMITY_PROVIDER_SOURCE_MISSING_OR_DUPLICATED",
        )

    def test_ninja_generates_manager_production_compile_command(self):
        compile_matches = [
            line for line in self.commands
            if " -c " in line
            and "vam_generation_manager.cc" in line
            and "vanetza_vam_obj.dir" in line
        ]

        self.assertEqual(
            len(compile_matches),
            1,
            "VAM_GENERATION_MANAGER_COMPILE_COMMAND_MISSING_OR_DUPLICATED\n"
            f"Matches: {compile_matches}",
        )

    def test_ninja_generates_manager_production_link_command(self):
        link_matches = [
            line for line in self.commands
            if " -c " not in line
            and " -o " in line
            and "vam_generation_manager.cc.o" in line
            and "vanetza_vam_obj.dir" in line
        ]

        self.assertEqual(
            len(link_matches),
            1,
            "VAM_GENERATION_MANAGER_OBJECT_MISSING_OR_DUPLICATED_IN_LINK\n"
            f"Matches: {link_matches}",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
