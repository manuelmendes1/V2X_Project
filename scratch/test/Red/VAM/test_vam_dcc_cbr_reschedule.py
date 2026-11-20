from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate

    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada")


def extract_observation(output: str, key: str) -> int:
    match = re.search(
        rf"^OBS {re.escape(key)}=(-?\d+)$",
        output,
        re.MULTILINE,
    )

    if not match:
        raise AssertionError(
            f"OBS {key} nao encontrado no output do harness"
        )

    return int(match.group(1))


def extract_cbr_snapshots(output: str) -> dict[int, dict]:
    snapshots = {}

    for line in output.splitlines():
        marker = "[DCC_CBR]"
        if marker not in line:
            continue

        line = line[line.index(marker):]

        sim_match = re.search(r"\bsim_us=(\d+)", line)
        cbr_match = re.search(r"\bcbr=([0-9.eE+-]+)", line)
        before_match = re.search(
            r"\bstate_before=(.*?)\s+state_after=",
            line,
        )
        after_match = re.search(
            r"\bstate_after=(.*?)\s+rx_start=",
            line,
        )

        if not all(
            (sim_match, cbr_match, before_match, after_match)
        ):
            continue

        sim_us = int(sim_match.group(1))

        snapshots[sim_us] = {
            "cbr": float(cbr_match.group(1)),
            "state_before": before_match.group(1).strip(),
            "state_after": after_match.group(1).strip(),
            "line": line,
        }

    return snapshots


class TestVamDccCbrReschedule(unittest.TestCase):
    def test_cbr_transition_reschedules_real_runtime_event(self):
        root = repository_root(Path(__file__))

        main_source = root / "scratch/v2x/v2x_sumo.cc"
        main_executable = root / "build/scratch/ns3.47-v2x_sumo-default"

        main_object = (
            "scratch/CMakeFiles/"
            "scratch_v2x_sumo.dir/v2x/v2x_sumo.cc.o"
        )

        test_source = (
            root
            / "scratch/test/Red/VAM/test_vam_dcc_cbr_reschedule.cc"
        )

        self.assertTrue(
            main_executable.is_file(),
            "Executavel ns-3 de referencia em falta",
        )
        self.assertTrue(
            test_source.is_file(),
            "Harness C++ em falta",
        )

        recipes = subprocess.run(
            [
                "ninja",
                "-C",
                "cmake-cache",
                "-t",
                "commands",
                "ns3.47-v2x_sumo-default",
            ],
            cwd=root,
            text=True,
            capture_output=True,
            timeout=45,
        )

        self.assertEqual(
            recipes.returncode,
            0,
            "Nao foi possivel obter receitas Ninja:\n"
            + recipes.stderr,
        )

        commands = recipes.stdout.splitlines()

        compile_commands = [
            line
            for line in commands
            if str(main_source) in line
            and re.search(r"\s-c\s", line)
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
            "Receita de compilacao nao unica",
        )
        self.assertEqual(
            len(link_commands),
            1,
            "Receita de link nao unica",
        )

        with tempfile.TemporaryDirectory(
            prefix="vam-dcc-cbr-reschedule-"
        ) as temporary:
            temp = Path(temporary)

            test_object = temp / "test_vam_dcc_cbr_reschedule.cc.o"
            test_binary = temp / "test_vam_dcc_cbr_reschedule"

            compile_command = compile_commands[0].replace(
                str(main_source),
                str(test_source),
            )

            compile_command = (
                compile_command
                .replace(
                    main_object + ".d",
                    str(test_object) + ".d",
                )
                .replace(
                    main_object,
                    str(test_object),
                )
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
                    "VAM_DCC_CBR_COMPILE=FAIL\n"
                    f"STDOUT:\n{compile_result.stdout[-6000:]}\n"
                    f"STDERR:\n{compile_result.stderr[-18000:]}"
                )

            link_command = (
                link_commands[0]
                .replace(
                    str(main_executable),
                    str(test_binary),
                )
                .replace(
                    main_object,
                    str(test_object),
                )
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
                    "VAM_DCC_CBR_LINK=FAIL\n"
                    f"STDOUT:\n{link_result.stdout[-6000:]}\n"
                    f"STDERR:\n{link_result.stderr[-18000:]}"
                )

            env = os.environ.copy()

            runtime_paths = [
                str(root / "build/lib"),
                str(Path.home() / "opt/vanetza-cpm-gcc11/lib"),
                str(Path.home() / "opt/geographiclib-gcc11/lib"),
                "/usr/local/lib",
            ]

            if env.get("LD_LIBRARY_PATH"):
                runtime_paths.append(env["LD_LIBRARY_PATH"])

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

            print("VAM_DCC_CBR_BUILD=PASS", flush=True)
            print(output, end="", flush=True)

            self.assertEqual(
                run.returncode,
                0,
                "O harness nao conseguiu observar as duas entregas "
                "ao Access:\n" + output[-12000:],
            )

            self.assertIn(
                "VAM_DCC_CBR_HARNESS=PASS",
                output,
            )

            first_access_us = extract_observation(
                output,
                "first_access_us",
            )

            second_access_us = extract_observation(
                output,
                "second_access_us",
            )

            snapshots = extract_cbr_snapshots(output)

            self.assertIn(
                100000,
                snapshots,
                "Falta medicao CBR aos 100 ms:\n" + output[-10000:],
            )

            self.assertIn(
                200000,
                snapshots,
                "Falta medicao CBR aos 200 ms:\n" + output[-10000:],
            )

            cbr_100 = snapshots[100000]
            cbr_200 = snapshots[200000]

            print(
                "OBS cbr_100ms="
                f"{cbr_100['cbr']:.6f} "
                f"state={cbr_100['state_before']}"
                f"->{cbr_100['state_after']}",
                flush=True,
            )

            print(
                "OBS cbr_200ms="
                f"{cbr_200['cbr']:.6f} "
                f"state={cbr_200['state_before']}"
                f"->{cbr_200['state_after']}",
                flush=True,
            )

            print(
                f"OBS expected_second_access_us=200000 "
                f"actual_second_access_us={second_access_us}",
                flush=True,
            )

            # Preconditions: prove that the intended CBR transitions
            # actually happened before interpreting the TX timestamp.
            self.assertGreaterEqual(
                cbr_100["cbr"],
                0.30,
                "A carga Wi-Fi nao atingiu CBR >= 0.30 aos 100 ms",
            )
            self.assertEqual(
                cbr_100["state_before"],
                "Relaxed",
            )
            self.assertEqual(
                cbr_100["state_after"],
                "Active 1",
            )

            self.assertLess(
                cbr_200["cbr"],
                0.30,
                "A carga Wi-Fi ainda nao tinha baixado aos 200 ms",
            )
            self.assertEqual(
                cbr_200["state_before"],
                "Active 1",
            )
            self.assertEqual(
                cbr_200["state_after"],
                "Relaxed",
            )

            self.assertEqual(
                first_access_us,
                20000,
                "A primeira entrega nao ocorreu aos 20 ms",
            )

            # This is the regression assertion.
            # Current code is expected to report 220000 us here.
            self.assertEqual(
                second_access_us,
                200000,
                "DCC CBR reschedule failure: a segunda transmissao "
                "deveria ser entregue ao Access aos 200 ms, "
                f"mas ocorreu aos {second_access_us} us",
            )

            print(
                "VAM_DCC_CBR_RESCHEDULE=PASS",
                flush=True,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
