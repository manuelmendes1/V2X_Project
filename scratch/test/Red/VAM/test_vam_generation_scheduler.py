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


class TestVamGenerationScheduler(unittest.TestCase):
    def test_observations_reach_generation_manager_evaluate(self):
        root = repository_root(Path(__file__))

        test_source = (
            root / "scratch/test/Red/VAM/"
            "test_vam_generation_scheduler.cc"
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
            test_object = temp / "test_vam_generation_scheduler.cc.o"
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
                "[PASS] RESET_CLEARS_PENDING_RUNTIME_ONLY",
                "[PASS] RESET_ALLOWS_FRESH_INITIAL_EVALUATION",
                "VAM_GENERATION_SCHEDULER=PASS",
            ):
                self.assertIn(marker, output, f"Missing evidence: {marker}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
