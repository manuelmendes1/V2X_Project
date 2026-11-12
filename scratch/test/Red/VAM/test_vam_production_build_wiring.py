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


class TestVamProductionBuildWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.cmake = (
            cls.root / "scratch" / "CMakeLists.txt"
        ).read_text(encoding="utf-8")

    def test_provider_belongs_to_dedicated_vam_object_library(self):
        start = self.cmake.find("add_library(vanetza_vam_obj OBJECT")
        self.assertGreaterEqual(
            start, 0, "VAM_OBJECT_LIBRARY_DECLARATION_MISSING"
        )

        end = self.cmake.find(
            "target_include_directories(vanetza_vam_obj PUBLIC", start
        )
        self.assertGreater(end, start)

        library_sources = self.cmake[start:end]
        self.assertIn(
            "${CMAKE_SOURCE_DIR}/scratch/"
            "vanetza_integration/vam/perception/"
            "vam_proximity_provider.cc",
            library_sources,
            "VAM_PROXIMITY_PROVIDER_SOURCE_NOT_REGISTERED",
        )

    def test_vam_object_library_has_project_include_paths(self):
        start = self.cmake.find(
            "target_include_directories(vanetza_vam_obj PUBLIC"
        )
        self.assertGreaterEqual(
            start, 0, "VAM_OBJECT_LIBRARY_INCLUDE_CONFIGURATION_MISSING"
        )

        end = self.cmake.find(")", start)
        include_configuration = self.cmake[start:end]

        self.assertIn(
            "${CMAKE_SOURCE_DIR}/scratch",
            include_configuration,
            "VAM_SCRATCH_INCLUDE_PATH_MISSING",
        )
        self.assertIn(
            "${CMAKE_SOURCE_DIR}/scratch/vanetza_integration",
            include_configuration,
            "VAM_INTEGRATION_INCLUDE_PATH_MISSING",
        )

    def test_scratch_executables_link_vam_object_library(self):
        start = self.cmake.find("function(create_scratch source_files)")
        self.assertGreaterEqual(
            start, 0, "SCRATCH_EXECUTABLE_BUILDER_MISSING"
        )

        end = self.cmake.find("endfunction()", start)
        self.assertGreater(end, start)

        builder = self.cmake[start:end]
        self.assertIn(
            "$<TARGET_OBJECTS:vanetza_vam_obj>",
            builder,
            "VAM_OBJECT_LIBRARY_NOT_LINKED_TO_SCRATCH_EXECUTABLES",
        )

    def test_ninja_generates_provider_compile_and_link_commands(self):
        configure = subprocess.run(
            ["cmake", "-S", ".", "-B", "cmake-cache"],
            cwd=self.root,
            capture_output=True,
            text=True,
            timeout=120,
        )
        self.assertEqual(
            configure.returncode,
            0,
            "VAM_PRODUCTION_CMAKE_RECONFIGURE_FAILED\n"
            f"STDOUT:\n{configure.stdout}\n"
            f"STDERR:\n{configure.stderr}",
        )

        result = subprocess.run(
            [
                "ninja", "-C", "cmake-cache",
                "-t", "commands",
                "ns3.47-v2x_sumo-default",
            ],
            cwd=self.root,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(
            result.returncode,
            0,
            "NINJA_PRODUCTION_COMMAND_QUERY_FAILED\n"
            f"STDOUT:\n{result.stdout}\n"
            f"STDERR:\n{result.stderr}",
        )

        commands = result.stdout.splitlines()

        compile_matches = [
            line for line in commands
            if " -c " in line
            and "vam_proximity_provider.cc" in line
            and "vanetza_vam_obj.dir" in line
        ]
        self.assertEqual(
            len(compile_matches),
            1,
            "VAM_PROVIDER_COMPILE_COMMAND_MISSING_OR_DUPLICATED\n"
            f"Matches: {compile_matches}",
        )

        link_matches = [
            line for line in commands
            if " -c " not in line
            and " -o " in line
            and "vam_proximity_provider.cc.o" in line
            and "vanetza_vam_obj.dir" in line
        ]
        self.assertEqual(
            len(link_matches),
            1,
            "VAM_PROVIDER_OBJECT_MISSING_OR_DUPLICATED_IN_LINK\n"
            f"Matches: {link_matches}",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
