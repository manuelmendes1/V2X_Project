#!/usr/bin/env python3

from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / ".git").exists()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("NS3_REPOSITORY_ROOT_NOT_FOUND")


class TestVamVruEndpointLifecycle(unittest.TestCase):
    def test_real_sumo_udp_endpoint_lifecycle_and_reentry(self):
        root = repository_root(Path(__file__))
        test_source = root / "scratch/test/Red/VAM/test_vam_vru_endpoint_lifecycle.cc"
        main_source = root / "scratch/v2x/v2x_sumo.cc"
        main_executable = root / "build/scratch/ns3.47-v2x_sumo-default"
        main_object = "scratch/CMakeFiles/scratch_v2x_sumo.dir/v2x/v2x_sumo.cc.o"
        target = "ns3.47-v2x_sumo-default"

        self.assertTrue(test_source.is_file(), "VAM_VRU_ENDPOINT_LIFECYCLE_CPP_HARNESS_MISSING")
        self.assertTrue(main_source.is_file(), "VAM_PRODUCTION_MAIN_SOURCE_MISSING")

        configure = subprocess.run(
            ["cmake", "-S", ".", "-B", "cmake-cache"],
            cwd=root,
            text=True,
            capture_output=True,
            timeout=120,
        )
        self.assertEqual(
            configure.returncode,
            0,
            "VAM_VRU_ENDPOINT_LIFECYCLE_CMAKE_CONFIGURE_FAILED\n"
            f"STDOUT:\n{configure.stdout}\nSTDERR:\n{configure.stderr}",
        )

        production_build = subprocess.run(
            ["ninja", "-C", "cmake-cache", "-j2", target],
            cwd=root,
            text=True,
            capture_output=True,
            timeout=300,
        )
        self.assertEqual(
            production_build.returncode,
            0,
            "VAM_VRU_ENDPOINT_LIFECYCLE_PRODUCTION_BUILD_FAILED\n"
            f"STDOUT:\n{production_build.stdout[-8000:]}\n"
            f"STDERR:\n{production_build.stderr[-16000:]}",
        )
        self.assertTrue(main_executable.is_file(), "VAM_PRODUCTION_EXECUTABLE_MISSING_AFTER_BUILD")

        command_result = subprocess.run(
            ["ninja", "-C", "cmake-cache", "-t", "commands", target],
            cwd=root,
            text=True,
            capture_output=True,
            timeout=30,
        )
        self.assertEqual(
            command_result.returncode,
            0,
            "VAM_VRU_ENDPOINT_LIFECYCLE_NINJA_COMMAND_QUERY_FAILED\n"
            f"STDOUT:\n{command_result.stdout}\nSTDERR:\n{command_result.stderr}",
        )

        commands = command_result.stdout.splitlines()
        compile_commands = [
            line for line in commands
            if str(main_source) in line and re.search(r"\s-c\s", line)
        ]
        link_commands = [
            line for line in commands
            if str(main_executable) in line and main_object in line and " -o " in line
        ]
        self.assertEqual(len(compile_commands), 1, "VAM_LIFECYCLE_PRODUCTION_COMPILE_RECIPE_NOT_UNIQUE")
        self.assertEqual(len(link_commands), 1, "VAM_LIFECYCLE_PRODUCTION_LINK_RECIPE_NOT_UNIQUE")

        with tempfile.TemporaryDirectory(prefix="vam-vru-lifecycle-") as temporary:
            temp = Path(temporary)
            test_object = temp / "test_vam_vru_endpoint_lifecycle.cc.o"
            test_binary = temp / "test_vam_vru_endpoint_lifecycle"

            compile_command = compile_commands[0]
            compile_command = compile_command.replace(str(main_source), str(test_source))
            compile_command = compile_command.replace(main_object + ".d", str(test_object) + ".d")
            compile_command = compile_command.replace(main_object, str(test_object))
            compile_result = subprocess.run(
                compile_command,
                shell=True,
                executable="/bin/bash",
                cwd=root / "cmake-cache",
                text=True,
                capture_output=True,
                timeout=240,
            )
            if compile_result.returncode != 0:
                self.fail(
                    "VAM_VRU_ENDPOINT_LIFECYCLE_HARNESS_COMPILE=FAIL\n"
                    f"STDOUT:\n{compile_result.stdout[-8000:]}\n"
                    f"STDERR:\n{compile_result.stderr[-16000:]}"
                )

            link_command = link_commands[0]
            link_command = link_command.replace(str(main_executable), str(test_binary))
            link_command = link_command.replace(main_object, str(test_object))
            link_result = subprocess.run(
                link_command,
                shell=True,
                executable="/bin/bash",
                cwd=root / "cmake-cache",
                text=True,
                capture_output=True,
                timeout=240,
            )
            if link_result.returncode != 0:
                self.fail(
                    "VAM_VRU_ENDPOINT_LIFECYCLE_HARNESS_LINK=FAIL\n"
                    f"STDOUT:\n{link_result.stdout[-8000:]}\n"
                    f"STDERR:\n{link_result.stderr[-16000:]}"
                )

            env = os.environ.copy()
            runtime_paths = [
                str(root / "build/lib"),
                str(Path.home() / "opt/vanetza-cpm-gcc11/lib"),
                str(Path.home() / "opt/geographiclib-gcc11/lib"),
                "/usr/local/lib",
            ]
            existing = env.get("LD_LIBRARY_PATH", "")
            if existing:
                runtime_paths.append(existing)
            env["LD_LIBRARY_PATH"] = ":".join(runtime_paths)

            run = subprocess.run(
                [str(test_binary)],
                cwd=temp,
                env=env,
                text=True,
                capture_output=True,
                timeout=60,
            )
            output = run.stdout + run.stderr
            print(f"VAM_VRU_ENDPOINT_LIFECYCLE_RUNTIME_RC={run.returncode}")
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                "VAM VRU endpoint lifecycle harness failed:\n"
                + "\n".join(output.splitlines()[-160:]),
            )

            expected_markers = (
                "[PASS] VAM_SCHEDULER_CONFIGURED",
                "[PASS] VRU_SCENARIO_IDENTITY_RESERVED",
                "[PASS] REAL_SUMO_UDP_ADAPTER_INITIALIZED",
                "[PASS] FIRST_UPSERT_CREATED_ACTIVE_PEDESTRIAN_ENDPOINT",
                "[PASS] FIRST_UPSERT_APPLIED_SUMO_HEADING_90_EAST_VELOCITY",
                "[PASS] REPEATED_UPSERT_REUSED_SAME_NETWORK_ENDPOINT",
                "[PASS] REPEATED_UPSERT_UPDATED_POSITION_AND_HEADING_0_NORTH_VELOCITY",
                "[PASS] REJECTED_STALE_UPSERT_LEFT_ENDPOINT_AND_MOBILITY_UNCHANGED",
                "[PASS] REJECTED_STALE_REMOVE_PRESERVED_ACTIVE_ENDPOINT",
                "[PASS] VALID_REMOVE_DESTROYED_NETWORK_ENDPOINT_AND_ACTIVE_SUMO_SUBJECT",
                "[PASS] VALID_REMOVE_PRESERVED_GLOBAL_STATION_IDENTITY",
                "[PASS] REENTRY_CREATED_NEW_ENDPOINT_WITH_PERSISTED_IDENTITY",
                "[PASS] REENTRY_APPLIED_FRESH_SUMO_MOBILITY_SAMPLE",
                "OLD_PRESENCE_ENDPOINT_CREATED",
                "NEW_PRESENCE_ENDPOINT_CREATED",
                "CONCURRENT_PRESENCE_ENDPOINTS_CREATED_WITH_DISTINCT_EPOCHS",
                "CONCURRENT_PRESENCE_ENDPOINT_NETWORKS_INITIALIZED",
                "COEXISTING_PRESENCE_ENDPOINTS_HAVE_DISTINCT_IPV4_REGISTRATIONS",
                "DESTROYING_OLD_ENDPOINT_PRESERVES_NEW_REGISTRY_ENTRY",
                "COEXISTENT_ENDPOINT_CLEANUP_PRESERVES_LOGICAL_STATION_IDENTITY",
                "[PASS] OWNERSHIP_STORE_INSERTS_ACTIVE_ENDPOINT",
                "[PASS] OWNERSHIP_STORE_RETAINS_ENDPOINT_FOR_OWNED_PRESENCE",
                "[PASS] OWNERSHIP_STORE_ADMITS_NEW_PRESENCE_WITH_RETIRED_PREDECESSOR",
                "[PASS] OWNERSHIP_STORE_REPEATED_UPSERT_REUSES_ONLY_MATCHING_EPOCH",
                "[PASS] OWNERSHIP_STORE_PRUNING_PRESERVES_STILL_OWNED_EPOCH",
                "[PASS] OWNERSHIP_STORE_PRUNING_RELEASES_ONLY_RESOLVED_OLD_EPOCH",
                "[PASS] OWNERSHIP_STORE_DESTROYS_ENDPOINT_WITHOUT_RETAINED_RUNTIME",
                "[PASS] OWNERSHIP_STORE_ENDPOINT_RETIREMENT_PRESERVES_ITS_IDENTITY",
                "VAM_VRU_ENDPOINT_OWNERSHIP_STORE=PASS",
                "VAM_VRU_ENDPOINT_LIFECYCLE_RUNTIME=PASS",
            )
            for marker in expected_markers:
                self.assertIn(marker, output, f"Missing evidence: {marker}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
