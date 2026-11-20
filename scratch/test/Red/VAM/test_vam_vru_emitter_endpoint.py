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


class TestVamVruEmitterEndpoint(unittest.TestCase):
    def test_real_non_vehicle_station_identity_network_mobility_and_reentry(self):
        root = repository_root(Path(__file__))

        test_source = (
            root
            / "scratch/test/Red/VAM/"
            "test_vam_vru_emitter_endpoint.cc"
        )
        main_source = root / "scratch/v2x/v2x_sumo.cc"
        main_executable = root / "build/scratch/ns3.47-v2x_sumo-default"
        main_object = (
            "scratch/CMakeFiles/scratch_v2x_sumo.dir/"
            "v2x/v2x_sumo.cc.o"
        )
        target = "ns3.47-v2x_sumo-default"

        self.assertTrue(
            test_source.is_file(),
            "VAM_VRU_ENDPOINT_CPP_HARNESS_MISSING",
        )
        self.assertTrue(
            main_source.is_file(),
            "VAM_PRODUCTION_MAIN_SOURCE_MISSING",
        )

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
            "VAM_VRU_ENDPOINT_CMAKE_CONFIGURE_FAILED\n"
            f"STDOUT:\n{configure.stdout}\n"
            f"STDERR:\n{configure.stderr}",
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
            "VAM_VRU_ENDPOINT_PRODUCTION_BUILD_FAILED\n"
            f"STDOUT:\n{production_build.stdout[-8000:]}\n"
            f"STDERR:\n{production_build.stderr[-16000:]}",
        )
        self.assertTrue(
            main_executable.is_file(),
            "VAM_PRODUCTION_EXECUTABLE_MISSING_AFTER_BUILD",
        )

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
            "VAM_VRU_ENDPOINT_NINJA_COMMAND_QUERY_FAILED\n"
            f"STDOUT:\n{command_result.stdout}\n"
            f"STDERR:\n{command_result.stderr}",
        )

        commands = command_result.stdout.splitlines()
        compile_commands = [
            line
            for line in commands
            if str(main_source) in line and re.search(r"\s-c\s", line)
        ]
        link_commands = [
            line
            for line in commands
            if str(main_executable) in line
            and main_object in line
            and " -o " in line
        ]

        self.assertEqual(
            len(compile_commands),
            1,
            "VAM_VRU_ENDPOINT_PRODUCTION_COMPILE_RECIPE_NOT_UNIQUE",
        )
        self.assertEqual(
            len(link_commands),
            1,
            "VAM_VRU_ENDPOINT_PRODUCTION_LINK_RECIPE_NOT_UNIQUE",
        )

        with tempfile.TemporaryDirectory(
            prefix="vam-vru-emitter-endpoint-"
        ) as temporary:
            temp = Path(temporary)
            test_object = temp / "test_vam_vru_emitter_endpoint.cc.o"
            test_binary = temp / "test_vam_vru_emitter_endpoint"

            compile_command = compile_commands[0]
            compile_command = compile_command.replace(
                str(main_source), str(test_source)
            )
            compile_command = compile_command.replace(
                main_object + ".d",
                str(test_object) + ".d",
            )
            compile_command = compile_command.replace(
                main_object,
                str(test_object),
            )

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
                    "VAM_VRU_ENDPOINT_HARNESS_COMPILE=FAIL\n"
                    f"STDOUT:\n{compile_result.stdout[-8000:]}\n"
                    f"STDERR:\n{compile_result.stderr[-16000:]}"
                )

            link_command = link_commands[0]
            link_command = link_command.replace(
                str(main_executable), str(test_binary)
            )
            link_command = link_command.replace(
                main_object, str(test_object)
            )

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
                    "VAM_VRU_ENDPOINT_HARNESS_LINK=FAIL\n"
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
            print(f"VAM_VRU_ENDPOINT_RUNTIME_RC={run.returncode}")
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                "VAM VRU emitter endpoint harness failed:\n"
                + "\n".join(output.splitlines()[-160:]),
            )

            expected_markers = (
                "[PASS] SCENARIO_IDENTITY_PRE_RESERVED_EXPLICITLY",
                "[PASS] EMPTY_PERSON_ID_REJECTED",
                "[PASS] ZERO_STATION_ID_REJECTED",
                "[PASS] NON_FINITE_INITIAL_POSITION_REJECTED",
                "[PASS] NON_FINITE_INITIAL_VELOCITY_REJECTED",
                "[PASS] CONFLICT_FIXTURE_IDENTITY_RESERVED",
                "[PASS] STATION_ID_OWNED_BY_ANOTHER_PERSON_REJECTED",
                "[PASS] EXISTING_PERSON_CANNOT_CHANGE_STATION_ID",
                "[PASS] PRE_RESERVED_VRU_ENDPOINT_CREATED",
                "[PASS] LOGICAL_ID_USES_CANONICAL_VAM_VRU_OWNER",
                "[PASS] EXPLICIT_STATION_ID_PRESERVED",
                "[PASS] GEO_NETWORK_STATION_TYPE_IS_PEDESTRIAN",
                "[PASS] ITS_STATION_STATE_IDENTITY_IS_CONSISTENT",
                "[PASS] ENDPOINT_USES_SCENARIO_IDENTITY_RESERVATION",
                "[PASS] VRU_NETWORK_INITIALIZED",
                "[PASS] VRU_NODE_SOCKET_IPV4_AND_LINK_MAC_CREATED",
                "[PASS] IPV4_REGISTRY_CONTAINS_NON_VEHICLE_PEDESTRIAN_ENDPOINT",
                "[PASS] VRU_ENDPOINT_DOES_NOT_CREATE_VEHICLE_REGISTRY_ENTRY",
                "[PASS] VRU_ENDPOINT_INSTALLS_CONSTANT_VELOCITY_MOBILITY",
                "[PASS] INITIAL_SUMO_NS3_POSITION_APPLIED",
                "[PASS] INITIAL_VELOCITY_SURVIVES_NETWORK_POSITION_SETUP",
                "[PASS] VALID_MOBILITY_UPDATE_ACCEPTED",
                "[PASS] MOBILITY_UPDATE_APPLIES_POSITION",
                "[PASS] MOBILITY_UPDATE_APPLIES_VELOCITY_AFTER_POSITION",
                "[PASS] NON_FINITE_MOBILITY_UPDATE_REJECTED",
                "[PASS] REJECTED_MOBILITY_UPDATE_PRESERVES_LAST_VALID_STATE",
                "[PASS] NETWORK_INITIALIZATION_IS_IDEMPOTENT",
                "[PASS] REPEATED_INITIALIZATION_DOES_NOT_CREATE_SECOND_NODE",
                "[PASS] MOBILITY_UPDATES_KEEP_VEHICLE_REGISTRY_UNTOUCHED",
                "[PASS] ENDPOINT_DESTRUCTION_UNREGISTERS_IPV4_ENDPOINT",
                "[PASS] ENDPOINT_DESTRUCTION_PRESERVES_GLOBAL_IDENTITY_RESERVATION",
                "[PASS] ENDPOINT_DESTRUCTION_DOES_NOT_CREATE_VEHICLE_STATE",
                "[PASS] SAME_LOGICAL_VRU_CAN_REENTER",
                "[PASS] REENTRY_REUSES_SAME_LOGICAL_STATION_IDENTITY",
                "[PASS] REENTRY_CREATES_NEW_NETWORK_ENDPOINT",
                "[PASS] REENTRY_REGISTERS_NEW_NODE_WITH_PERSISTED_IDENTITY",
                "[PASS] REENTRY_REMAINS_OUTSIDE_VEHICLE_REGISTRY",
                "[PASS] REENTRY_ENDPOINT_DESTRUCTION_STILL_PRESERVES_IDENTITY",
                "VAM_VRU_ENDPOINT_RUNTIME=PASS",
            )
            for marker in expected_markers:
                self.assertIn(marker, output, f"Missing evidence: {marker}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
