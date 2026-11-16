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


class TestVamScenarioSubjectRuntimeUdp(unittest.TestCase):
    def test_real_sumo_udp_callbacks_drive_vam_scenario_runtime(self):
        root = repository_root(Path(__file__))

        test_source = (
            root
            / "scratch/test/Red/VAM/"
            "test_vam_scenario_subject_runtime_udp.cc"
        )
        main_source = root / "scratch/v2x/v2x_sumo.cc"
        main_executable = (
            root / "build/scratch/ns3.47-v2x_sumo-default"
        )
        main_object = (
            "scratch/CMakeFiles/scratch_v2x_sumo.dir/"
            "v2x/v2x_sumo.cc.o"
        )
        target = "ns3.47-v2x_sumo-default"

        self.assertTrue(
            test_source.is_file(),
            "VAM_SCENARIO_UDP_CPP_HARNESS_MISSING",
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
            "VAM_SCENARIO_UDP_CMAKE_CONFIGURE_FAILED\n"
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
            "VAM_SCENARIO_UDP_PRODUCTION_BUILD_FAILED\n"
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
            "VAM_SCENARIO_UDP_NINJA_COMMAND_QUERY_FAILED\n"
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
            "VAM_SCENARIO_UDP_PRODUCTION_COMPILE_RECIPE_NOT_UNIQUE",
        )
        self.assertEqual(
            len(link_commands),
            1,
            "VAM_SCENARIO_UDP_PRODUCTION_LINK_RECIPE_NOT_UNIQUE",
        )

        with tempfile.TemporaryDirectory(
            prefix="vam-scenario-udp-"
        ) as temporary:
            temp = Path(temporary)
            test_object = temp / "test_vam_scenario_subject_runtime_udp.cc.o"
            test_binary = temp / "test_vam_scenario_subject_runtime_udp"

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
                    "VAM_SCENARIO_UDP_HARNESS_COMPILE=FAIL\n"
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
                    "VAM_SCENARIO_UDP_HARNESS_LINK=FAIL\n"
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
                timeout=45,
            )

            output = run.stdout + run.stderr
            print(f"VAM_SCENARIO_UDP_RUNTIME_RC={run.returncode}")
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                "VAM scenario UDP runtime harness failed:\n"
                + "\n".join(output.splitlines()[-120:]),
            )

            expected_markers = (
                "[PASS] INITIAL_VAM_SCHEDULER_CONFIGURED",
                "[PASS] INVALID_ZERO_CHECK_INTERVAL_REJECTED",
                "[PASS] INVALID_CHECK_INTERVAL_ABOVE_MIN_REJECTED",
                "[PASS] INVALID_ZERO_MIN_INTERVAL_REJECTED",
                "[PASS] INVALID_MAX_GENERATION_INTERVAL_REJECTED",
                "[PASS] INVALID_LOW_FREQUENCY_INTERVAL_REJECTED",
                "[PASS] INVALID_SKIP_COUNT_BELOW_RANGE_REJECTED",
                "[PASS] INVALID_SKIP_COUNT_ABOVE_RANGE_REJECTED",
                "[PASS] INVALID_POSITION_CHANGE_THRESHOLD_REJECTED",
                "[PASS] INVALID_GROUND_SPEED_THRESHOLD_REJECTED",
                "[PASS] INVALID_ORIENTATION_THRESHOLD_REJECTED",
                "[PASS] INVALID_INTERCEPTION_THRESHOLD_REJECTED",
                "[PASS] INVALID_PROXIMITY_THRESHOLD_REJECTED",
                "[PASS] ATOMIC_SCENARIO_ID_CONFLICT_REJECTED",
                "[PASS] ATOMIC_ROLLBACK_RELEASED_NEW_RESERVATION",
                "[PASS] ATOMIC_ROLLBACK_PRESERVED_EXISTING_OWNER",
                "[PASS] SCENARIO_SUBJECT_HAS_EXPLICIT_GLOBAL_STATION_ID",
                "[PASS] REAL_SUMO_ADAPTER_INITIALIZED",
                "[PASS] UDP_UPSERT_APPLIED_SCENARIO_PROFILE_AND_ROLE",
                "[PASS] UDP_STEP_END_SENT",
                "[PASS] FIRST_EVALUATION_ONLY_CANDIDATE_ABORTED",
                "[PASS] SECOND_EVALUATION_ONLY_CANDIDATE_ABORTED",
                "[PASS] REPEATED_UPSERT_DID_NOT_REACTIVATE_DEACTIVATED_SUBJECT",
                "[PASS] VALID_SCHEDULER_RECONFIGURATION_ACCEPTED",
                "[PASS] REMOVE_CLEARED_PERSON_AND_VRU_SUBJECT",
                "[PASS] REMOVE_PRESERVED_LOGICAL_STATION_RESERVATION",
                "[PASS] REENTRY_REAPPLIED_SCENARIO_PROFILE_AND_ROLE",
                "[PASS] REENTRY_EVALUATION_ONLY_CANDIDATE_ABORTED",
                "VAM_SCENARIO_UDP_RUNTIME=PASS",
            )
            for marker in expected_markers:
                self.assertIn(marker, output, f"Missing evidence: {marker}")

            evaluated_lines = [
                line for line in output.splitlines()
                if "[VAM_GENERATION] CANDIDATE_EVALUATED" in line
            ]
            aborted_lines = [
                line for line in output.splitlines()
                if "[VAM_GENERATION] CANDIDATE_ABORTED" in line
            ]

            self.assertEqual(
                len(evaluated_lines), 3,
                "Expected initial, retry and reentry candidate evaluations",
            )
            self.assertEqual(
                len(aborted_lines), 3,
                "Every evaluation-only candidate must be aborted",
            )
            self.assertTrue(
                all("person_id=runtime-vru" in line for line in evaluated_lines),
                "Candidate evaluation escaped the configured scenario subject",
            )
            self.assertTrue(
                all("station_id=62101" in line for line in evaluated_lines),
                "Candidate evaluation did not preserve the configured station ID",
            )
            self.assertTrue(
                all(
                    "role_on=true" in line
                    and "vbs_active_standalone=true" in line
                    for line in evaluated_lines
                ),
                "Candidate evaluation did not preserve the authorized VRU role",
            )
            self.assertFalse(
                any("CANDIDATE_ABORT_FAILED" in line for line in output.splitlines()),
                "Controller failed to resolve an evaluation-only candidate",
            )
            self.assertFalse(
                any("CANDIDATE_PENDING" in line for line in output.splitlines()),
                "Evaluation-only candidate was left pending",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
