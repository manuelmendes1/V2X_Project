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


class TestVamTransactionLifecycle(unittest.TestCase):
    def test_transaction_lifecycle_rejects_stale_identity(self):
        root = repository_root(Path(__file__))

        test_source = (
            root / "scratch/test/Red/VAM/"
            "test_vam_transaction_lifecycle.cc"
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
            "VAM_GENERATION_EVALUATION_CPP_HARNESS_MISSING",
        )
        self.assertTrue(
            main_executable.is_file(),
            "VAM_PRODUCTION_EXECUTABLE_MISSING",
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
            "VAM_SCHEDULER_CMAKE_CONFIGURE_FAILED\n"
            f"STDOUT:\n{configure.stdout}\n"
            f"STDERR:\n{configure.stderr}",
        )

        production_build = subprocess.run(
            ["ninja", "-C", "cmake-cache", "-j2", target],
            cwd=root,
            text=True,
            capture_output=True,
            timeout=240,
        )
        self.assertEqual(
            production_build.returncode,
            0,
            "VAM_SCHEDULER_PRODUCTION_BUILD_FAILED\n"
            f"STDOUT:\n{production_build.stdout}\n"
            f"STDERR:\n{production_build.stderr}",
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
            "VAM_SCHEDULER_NINJA_COMMAND_QUERY_FAILED\n"
            f"STDOUT:\n{command_result.stdout}\n"
            f"STDERR:\n{command_result.stderr}",
        )

        commands = command_result.stdout.splitlines()
        compile_commands = [
            line for line in commands
            if str(main_source) in line and re.search(r"\s-c\s", line)
        ]
        link_commands = [
            line for line in commands
            if str(main_executable) in line
            and main_object in line
            and " -o " in line
        ]

        self.assertEqual(
            len(compile_commands),
            1,
            "VAM_SCHEDULER_PRODUCTION_COMPILE_RECIPE_NOT_UNIQUE",
        )
        self.assertEqual(
            len(link_commands),
            1,
            "VAM_SCHEDULER_PRODUCTION_LINK_RECIPE_NOT_UNIQUE",
        )

        with tempfile.TemporaryDirectory(
            prefix="vam-generation-scheduler-"
        ) as temporary:
            temp = Path(temporary)
            test_object = temp / "test_vam_transaction_lifecycle.cc.o"
            test_binary = temp / "test_vam_generation_scheduler"

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
                timeout=180,
            )
            if compile_result.returncode != 0:
                self.fail(
                    "VAM_SCHEDULER_HARNESS_COMPILE=FAIL\n"
                    f"STDOUT:\n{compile_result.stdout[-5000:]}\n"
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
                timeout=180,
            )
            if link_result.returncode != 0:
                self.fail(
                    "VAM_SCHEDULER_HARNESS_LINK=FAIL\n"
                    f"STDOUT:\n{link_result.stdout[-5000:]}\n"
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
                timeout=35,
            )

            output = run.stdout + run.stderr
            print(f"VAM_SCHEDULER_RUNTIME_RC={run.returncode}")
            print(output, end="")

            self.assertEqual(
                run.returncode,
                0,
                "VAM generation scheduler harness failed:\n"
                + "\n".join(output.splitlines()[-100:]),
            )

            for marker in (
                "[PASS] SUBJECT_REQUIRES_EXPLICIT_STATION_ID",
                "[PASS] VAM_IDENTITY_IS_RESERVED_BY_ITS_AUTHORITY",
                "[PASS] INITIAL_RUNTIME_EVALUATION_OPENS_CANDIDATE",
                "[PASS] CANDIDATE_REMAINS_PENDING_UNTIL_RESOLVED",
                "[PASS] EACH_ACTIVE_SUBJECT_HAS_AN_INDEPENDENT_MANAGER",
                "[PASS] ABORTED_SUBJECT_CAN_RETRY_WITHOUT_RESETTING_CONFIRMED_PEER",
                "[PASS] CHECK_INTERVAL_USES_LOGICAL_SUMO_TIME",
                "[PASS] TIME_REGRESSION_IS_REJECTED_WITHOUT_OUTPUT_MUTATION",
                "[PASS] REMOVE_FORGETS_RUNTIME_MANAGER_AND_PENDING_CANDIDATE",
                "[PASS] REENTRY_STARTS_NEW_RUNTIME_GENERATION_HISTORY",
                "[PASS] RESET_FIXTURE_HAS_PENDING_CANDIDATE",
                "[PASS] DRAIN_CLEARS_RUNTIME_AND_PRESERVES_IDENTITY",
                "[PASS] DRAIN_ALLOWS_FRESH_INITIAL_EVALUATION",
                "STALE_PRESENCE_TRANSACTION_CANNOT_CONFIRM_REENTRY",
                "STALE_PRESENCE_TRANSACTION_CANNOT_ABORT_REENTRY",
                "REENTRY_HANDLE_DIFFERS_BY_PRESENCE_ONLY",
                "STALE_CONFIRMATION_DOES_NOT_MUTATE_NEW_PENDING_TRANSACTION",
                "CURRENT_PRESENCE_HANDLE_CONFIRMS_CURRENT_TRANSACTION",
                "CONFIRMED_TRANSACTION_HANDLE_CANNOT_BE_REUSED",
                "OLD_SCHEDULER_TRANSACTION_CANNOT_ABORT_NEW_SCHEDULER",
                "OLD_SCHEDULER_TRANSACTION_CANNOT_CONFIRM_NEW_SCHEDULER",
                "SCHEDULER_HANDLE_DIFFERS_BY_INSTANCE_ONLY",
                "STALE_ABORT_DOES_NOT_MUTATE_NEW_SCHEDULER_TRANSACTION",
                "CURRENT_SCHEDULER_HANDLE_ABORTS_CURRENT_TRANSACTION",
                "ABORTED_TRANSACTION_HANDLE_CANNOT_BE_REUSED",
                "SCHEDULER_INSTANCE_ID_LAST_VALUE_IS_ALLOCATED",
                "SCHEDULER_INSTANCE_ID_EXHAUSTION_NEVER_REUSES_VALUE",
                "STALE_PRESENCE_TRANSACTION_CANNOT_CONFIRM_REENTRY",
                "STALE_PRESENCE_TRANSACTION_CANNOT_ABORT_REENTRY",
                "REENTRY_HANDLE_DIFFERS_BY_PRESENCE_ONLY",
                "STALE_CONFIRMATION_DOES_NOT_MUTATE_NEW_PENDING_TRANSACTION",
                "CURRENT_PRESENCE_HANDLE_CONFIRMS_CURRENT_TRANSACTION",
                "CONFIRMED_TRANSACTION_HANDLE_CANNOT_BE_REUSED",
                "OLD_SCHEDULER_TRANSACTION_CANNOT_ABORT_NEW_SCHEDULER",
                "OLD_SCHEDULER_TRANSACTION_CANNOT_CONFIRM_NEW_SCHEDULER",
                "SCHEDULER_HANDLE_DIFFERS_BY_INSTANCE_ONLY",
                "STALE_ABORT_DOES_NOT_MUTATE_NEW_SCHEDULER_TRANSACTION",
                "CURRENT_SCHEDULER_HANDLE_ABORTS_CURRENT_TRANSACTION",
                "ABORTED_TRANSACTION_HANDLE_CANNOT_BE_REUSED",
                "DRAIN_RESTARTS_LOCAL_ID_WITH_NEW_INCARNATION",
                "DRAIN_ROTATES_SCHEDULER_INCARNATION",
                "OLD_INCARNATION_HANDLE_CANNOT_CONFIRM_AFTER_DRAIN",
                "DRAIN_STALE_HANDLE_DOES_NOT_MUTATE_NEW_PENDING_TRANSACTION",
                "NEW_HANDLE_ABORTS_AFTER_DRAIN",
                "SCHEDULER_INSTANCE_ID_LAST_VALUE_IS_ALLOCATED",
                "SCHEDULER_INSTANCE_ID_EXHAUSTION_NEVER_REUSES_VALUE",
                "VAM_GENERATION_SCHEDULER=PASS",
                "REMOVE_ABORTS_PENDING_TRANSACTION_IMMEDIATELY",
                "REMOVE_RETIRES_ONLY_REMOVED_PERSON_TRANSACTION",
                "REMOVE_REJECTS_LATE_CONFIRMATION_AND_ABORT",
                "REMOVE_PRESERVES_IDENTITY_RESERVATION",
                "FAILED_ABORT_PRESERVES_RETIRED_TRANSACTION",
                "RETIRED_HANDLE_REJECTS_LATE_RESOLUTION",
                "RETIRED_RETRY_RUNS_BEFORE_CADENCE_RETURN",
                "QUARANTINE_DOES_NOT_BLOCK_UNRELATED_SUBJECT",
                "RETIRED_ABORT_SUCCESS_RELEASES_QUARANTINE",
                "REENTRY_AFTER_RETIREMENT_CREATES_FRESH_INITIAL_MANAGER",
                "RETIRED_HANDLE_REMAINS_STALE_AFTER_RECOVERY",
                "U3_TRANSACTION_DRAIN_API_AVAILABLE",
                "COMPLETE_DRAIN_RESOLVES_ALL_PENDING_TRANSACTIONS",
                "COMPLETE_DRAIN_REJECTS_OLD_CONFIRMATION",
                "COMPLETE_DRAIN_REJECTS_OLD_ABORT",
                "COMPLETE_DRAIN_PRESERVES_ITS_IDENTITY_RESERVATION",
                "COMPLETE_DRAIN_IS_IDEMPOTENT",
                "PARTIAL_DRAIN_RETAINS_FAILED_ABORT_AND_RESOLVES_PEER",
               "PARTIAL_DRAIN_RETRY_ADVANCES_LOCAL_TRANSACTION_ID",
               "PARTIAL_DRAIN_RECOVERY_RESTORES_SAME_PRESENCE_MANAGER",
               "PARTIAL_DRAIN_RECOVERY_ADVANCES_LOCAL_TRANSACTION_ID",
               "PARTIAL_DRAIN_RETIRED_HANDLE_CANNOT_CONFIRM_RECOVERED_PRESENCE",
               "PARTIAL_DRAIN_RETIRED_HANDLE_CANNOT_ABORT_RECOVERED_PRESENCE",
               "PARTIAL_DRAIN_RECOVERY_STALE_HANDLE_PRESERVES_NEW_CANDIDATE",
               "PARTIAL_DRAIN_OLD_HANDLE_CANNOT_CONFIRM_NEW_TRANSACTION",
               "PARTIAL_DRAIN_OLD_HANDLE_CANNOT_ABORT_NEW_TRANSACTION",
               "PARTIAL_DRAIN_STALE_HANDLE_ATTEMPTS_PRESERVE_NEW_CANDIDATE",
               "PARTIAL_DRAIN_LATER_RETRY_COMPLETES",
               "PARTIAL_DRAIN_PRESERVES_PEER_IDENTITY_RESERVATIONS",
               "REPEATED_PARTIAL_DRAIN_FIRST_ATTEMPT_PARTIAL",
               "REPEATED_PARTIAL_DRAIN_SECOND_ATTEMPT_PARTIAL",
               "REPEATED_PARTIAL_DRAIN_RESTORES_RESOLVED_B_MANAGER",
               "REPEATED_PARTIAL_DRAIN_EVALUATION_ACCEPTED",
               "REPEATED_PARTIAL_DRAIN_B_COUNTER_ADVANCES",
               "REPEATED_PARTIAL_DRAIN_B_HANDLE_IS_DISTINCT",
               "REPEATED_PARTIAL_DRAIN_OLD_HANDLE_REJECTED",
               "REPEATED_PARTIAL_DRAIN_NEW_CANDIDATE_PRESERVED",
               "REPEATED_PARTIAL_DRAIN_FINAL_DRAIN_COMPLETES",
               "REPEATED_PARTIAL_DRAIN_PRESERVES_IDENTITIES",
                "PARTIAL_DRAIN_REJECTS_RETIRED_CONFIRMATION",
                "PARTIAL_DRAIN_REJECTS_RETIRED_ABORT",
                "PARTIAL_DRAIN_KEEPS_SUCCESSFULLY_ABORTED_PEER_RESOLVED",
                "PARTIAL_DRAIN_PRESERVES_ALL_IDENTITY_RESERVATIONS",
                "SECOND_DRAIN_RECOVERS_RETAINED_TRANSACTION",
                "RECOVERED_DRAIN_IS_IDEMPOTENT",
                "VAM_TRANSACTION_DRAIN=PASS",
                "[PASS] VAM_SUBMITTED_TRANSACTION_API_AVAILABLE",
                "[PASS] MARK_SUBMITTED_IS_EXACTLY_ONCE",
                "[PASS] SUBMITTED_DIRECT_CONFIRM_ABORT_REJECTED",
                "[PASS] REMOVE_RETIRES_SUBMITTED_WITHOUT_ABORT",
                "[PASS] REENTRY_COEXISTS_WITH_OLD_IN_FLIGHT_HANDLE",
                "[PASS] LATE_COMPLETION_RESOLVES_ONLY_OLD_PRESENCE",
                "[PASS] DUPLICATE_COMPLETION_CANNOT_MUTATE_REENTRY",
                "[PASS] FAILED_SUBMISSION_ABORTS_GENERATION",
                "[PASS] DRAIN_BLOCKS_WITHOUT_ABORTING_SUBMITTED_TRANSACTION",
                "[PASS] DRAIN_COMPLETES_AFTER_TERMINAL_CALLBACK",
                "[PASS] DRAIN_INVALIDATES_COMPLETED_SUBMITTED_HANDLE",
                "VAM_SUBMITTED_TRANSACTION_LIFECYCLE=PASS",
                "VAM_TRANSACTION_RETIREMENT=PASS",
                "VAM_TRANSACTION_LIFECYCLE=PASS",
                "VAM_TRANSACTION_LIFECYCLE=PASS",
            ):
                self.assertIn(marker, output, f"Missing evidence: {marker}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
