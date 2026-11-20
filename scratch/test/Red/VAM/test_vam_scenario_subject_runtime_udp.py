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
                "[PASS] SUMO_NETWORK_PROJECTION_LOADED_BY_RUNTIME_HARNESS",
                "[PASS] UDP_UPSERT_APPLIED_SCENARIO_PROFILE_AND_ROLE",
                "[PASS] UDP_STEP_END_SENT",
                "[PASS] UDP_BACKED_VAM_CANDIDATE_REBUILT",
                "[PASS] UDP_BACKED_VAM_PROFILE1_UPER_ENCODED",
                "[PASS] UDP_BACKED_VAM_R2_UPER_ROUNDTRIP_DECODED",
                "[PASS] UDP_BACKED_VAM_HEADER_STATION_ID_PRESERVED",
                "[PASS] UDP_BACKED_VAM_REFERENCE_TIME_PRESERVED",
                "[PASS] UDP_BACKED_VAM_WGS84_POSITION_PRESERVED",
                "[PASS] UDP_BACKED_VAM_PEDESTRIAN_TYPE_PRESERVED",
                "[PASS] UDP_BACKED_VAM_OBSERVER_TRANSACTION_ABORTED",
                "[PASS] FIRST_EVALUATION_ONLY_CANDIDATE_ABORTED",
                "[PASS] SECOND_EVALUATION_ONLY_CANDIDATE_ABORTED",
                "[PASS] REPEATED_UPSERT_DID_NOT_REACTIVATE_DEACTIVATED_SUBJECT",
                "[PASS] VALID_SCHEDULER_RECONFIGURATION_ACCEPTED",
                "[PASS] REMOVE_CLEARED_PERSON_AND_VRU_SUBJECT",
                "[PASS] REMOVE_PRESERVED_LOGICAL_STATION_RESERVATION",
                "[PASS] REENTRY_REAPPLIED_SCENARIO_PROFILE_AND_ROLE",
                "[PASS] VAM_ENDPOINT_WIFI_CHANNEL_INITIALIZED",
                "[PASS] FAULT_FIXTURE_ROLE_REACTIVATED",
                "[PASS] FAULT_FIXTURE_VBS_REACTIVATED",
                "[PASS] ABORT_FAULT_INJECTION_ARMED",
                "[PASS] OLD_PRESENCE_CANDIDATE_ABORT_FAILED_AND_RETAINED",
                "[PASS] REMOVE_RETAINED_OLD_PRESENCE_RUNTIME",
                "[PASS] RETIRED_RUNTIME_RETRY_STILL_QUARANTINED",
                "[PASS] RETIRED_RUNTIME_RECOVERY_COMPLETED",
                "VAM_SCENARIO_UDP_RUNTIME=PASS",
            )
            for marker in expected_markers:
                self.assertIn(marker, output, f"Missing evidence: {marker}")

            output_lines = output.splitlines()
            evaluated_lines = [
                line for line in output_lines
                if "[VAM_GENERATION] CANDIDATE_EVALUATED" in line
            ]
            encoded_lines = [
                line for line in output_lines
                if "[VAM_GENERATION] CANDIDATE_ENCODED" in line
            ]
            aborted_lines = [
                line for line in output_lines
                if "[VAM_GENERATION] CANDIDATE_ABORTED" in line
            ]
            abort_failed_lines = [
                line for line in output_lines
                if "[VAM_GENERATION] CANDIDATE_ABORT_FAILED" in line
            ]
            encode_failed_lines = [
                line for line in output_lines
                if "[VAM_GENERATION] CANDIDATE_ENCODE_FAILED" in line
            ]

            self.assertEqual(
                len(evaluated_lines), 4,
                "Expected two normal candidates, one failed old-presence candidate, "
                "and one recovered new-presence candidate",
            )
            self.assertEqual(
                len(encoded_lines), 4,
                "Every evaluated candidate must reach the production UPER encoder",
            )
            # SUMO reference time belongs to STEP_EVALUATED records. The
            # CANDIDATE_EVALUATED record carries generation_time_its_us instead.
            # Correlate each step with its candidate count so the quarantined
            # presence is explicitly proven to produce no candidate at 400000.
            step_evaluation_lines = [
                line for line in output_lines
                if "[VAM_GENERATION] STEP_EVALUATED" in line
            ]
            step_evaluation_times = []
            step_candidate_counts = []
            for line in step_evaluation_lines:
                time_match = re.search(r"reference_sumo_us=([0-9]+)", line)
                candidate_match = re.search(r"candidates=([0-9]+)", line)
                self.assertIsNotNone(time_match, line)
                self.assertIsNotNone(candidate_match, line)
                step_evaluation_times.append(int(time_match.group(1)))
                step_candidate_counts.append(int(candidate_match.group(1)))

            self.assertEqual(
                step_evaluation_times,
                [100000, 200000, 300000, 400000, 500000],
                "Expected the two normal steps, faulted old presence, "
                "quarantined retry, and post-recovery step",
            )
            self.assertEqual(
                step_candidate_counts, [1, 1, 1, 0, 1],
                "The re-entered presence must remain quarantined at 400000 "
                "and become eligible only after retired-runtime recovery at 500000",
            )
            evaluation_times = [
                reference_time
                for reference_time, candidate_count in zip(
                    step_evaluation_times, step_candidate_counts
                )
                if candidate_count > 0
            ]
            self.assertEqual(
                evaluation_times, [100000, 200000, 300000, 500000],
                "Candidate evaluation times must exclude the quarantined 400000 step",
            )
            self.assertEqual(
                len(aborted_lines), 3,
                "Two normal candidates and the recovered new-presence candidate "
                "must abort successfully",
            )
            self.assertEqual(
                len(abort_failed_lines), 2,
                "The controller must report the candidate abort failure and the "
                "accepted REMOVE invalidation failure",
            )
            self.assertEqual(
                encode_failed_lines, [],
                "All production candidates must pass UPER encoding",
            )

            def transaction_id(line):
                match = re.search(r"transaction_id=([0-9]+)", line)
                self.assertIsNotNone(match, line)
                return match.group(1)

            # Every evaluation is encoded after evaluation, preserving the exact
            # transaction identity. The third evaluation deliberately remains
            # un-aborted until REMOVE and retired-ledger recovery.
            cursor = 0
            for evaluated, encoded in zip(evaluated_lines, encoded_lines):
                self.assertEqual(transaction_id(encoded), transaction_id(evaluated))
                evaluated_pos = output_lines.index(evaluated, cursor)
                encoded_pos = output_lines.index(encoded, evaluated_pos + 1)
                self.assertLess(evaluated_pos, encoded_pos)
                cursor = encoded_pos + 1

            expected_successful_abort_evaluations = [
                evaluated_lines[0],
                evaluated_lines[1],
                evaluated_lines[3],
            ]
            # Transaction IDs are manager-local and restart at 1 for a new
            # presence epoch. Therefore identical CANDIDATE_ABORTED log lines
            # can occur more than once; match each occurrence after its own
            # evaluation instead of using list.index(), which returns the first
            # occurrence in the entire output.
            abort_search_cursor = 0
            for evaluated, aborted in zip(
                expected_successful_abort_evaluations, aborted_lines
            ):
                self.assertEqual(transaction_id(aborted), transaction_id(evaluated))
                evaluated_pos = output_lines.index(evaluated)
                search_from = max(abort_search_cursor, evaluated_pos + 1)
                aborted_pos = next(
                    (
                        position
                        for position in range(search_from, len(output_lines))
                        if output_lines[position] == aborted
                    ),
                    None,
                )
                self.assertIsNotNone(
                    aborted_pos,
                    "Expected matching CANDIDATE_ABORTED occurrence after its evaluation: "
                    + evaluated,
                )
                self.assertGreater(aborted_pos, evaluated_pos)
                abort_search_cursor = aborted_pos + 1
            # VAM_ABORT_DUPLICATE_TRANSACTION_LOG_MATCHING_V3

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

            remove_retained_lines = [
                line for line in output_lines
                if "[VAM_VRU_ENDPOINT] REMOVE_ACCEPTED" in line
                and "presence_epoch_us=1000" in line
                and "runtime_retained=yes" in line
            ]
            self.assertEqual(
                len(remove_retained_lines), 1,
                "REMOVE did not retain the endpoint of the old owned presence",
            )

            endpoint_ready_lines = [
                line for line in output_lines
                if "[VAM_VRU_ENDPOINT] ENDPOINT_READY" in line
                and "person_id=runtime-vru" in line
            ]
            self.assertEqual(
                len(endpoint_ready_lines), 2,
                "Expected one real network endpoint for each presence epoch",
            )

            endpoint_records = []
            for line in endpoint_ready_lines:
                epoch = re.search(r"presence_epoch_us=([0-9]+)", line)
                ipv4 = re.search(r"ipv4=([0-9.]+)", line)
                self.assertIsNotNone(epoch, line)
                self.assertIsNotNone(ipv4, line)
                endpoint_records.append((int(epoch.group(1)), ipv4.group(1)))
            self.assertEqual(
                [record[0] for record in endpoint_records], [1000, 5000],
                "Endpoint epochs do not match the old and re-entered SUMO presences",
            )
            self.assertNotEqual(
                endpoint_records[0][1], endpoint_records[1][1],
                "Concurrent/retained presence endpoints reused the same IPv4 address",
            )

            pruned_lines = [
                line for line in output_lines
                if "[VAM_VRU_ENDPOINT] RETIRED_ENDPOINTS_PRUNED" in line
                and "released=1" in line
            ]
            self.assertEqual(
                len(pruned_lines), 1,
                "Successful retired-runtime recovery must release exactly the old endpoint",
            )
            self.assertLess(
                output_lines.index(endpoint_ready_lines[0]),
                output_lines.index(remove_retained_lines[0]),
            )
            self.assertLess(
                output_lines.index(remove_retained_lines[0]),
                output_lines.index(endpoint_ready_lines[1]),
            )
            self.assertLess(
                output_lines.index(endpoint_ready_lines[1]),
                output_lines.index(pruned_lines[0]),
            )

            attempts_line = next(
                (line for line in output_lines
                 if line.startswith("VAM_ABORT_FAULT_INJECTION_ATTEMPTS=")),
                None,
            )
            self.assertEqual(
                attempts_line, "VAM_ABORT_FAULT_INJECTION_ATTEMPTS=5",
                "Expected three deterministic abort failures followed by two successes",
            )
            self.assertFalse(
                any("CANDIDATE_PENDING" in line for line in output_lines),
                "A VAM evaluation-only transaction remained pending after recovery",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
