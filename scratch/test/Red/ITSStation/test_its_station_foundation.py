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


class TestItsStationFoundation(unittest.TestCase):
    def test_state_address_registry_network_phy_and_access_contracts(self):
        root = repository_root(Path(__file__))
        test_source = (
            root / "scratch/test/Red/ITSStation/"
            "test_its_station_foundation.cc"
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

        self.assertTrue(test_source.is_file(), "ITS Station C++ harness em falta")
        self.assertTrue(main_executable.is_file(), "Executavel de referencia em falta")

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
            "Nao foi possivel obter as receitas Ninja:\n"
            + command_result.stderr,
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
            len(compile_commands), 1,
            "A receita de compilacao de v2x_sumo.cc nao e unica",
        )
        self.assertEqual(
            len(link_commands), 1,
            "A receita de link do executavel de producao nao e unica",
        )

        with tempfile.TemporaryDirectory(
            prefix="its-station-foundation-"
        ) as temporary:
            temp = Path(temporary)
            test_object = temp / "test_its_station_foundation.cc.o"
            test_binary = temp / "test_its_station_foundation"

            compile_command = compile_commands[0]
            compile_command = compile_command.replace(
                str(main_source), str(test_source)
            )
            compile_command = compile_command.replace(
                main_object + ".d", str(test_object) + ".d"
            )
            compile_command = compile_command.replace(
                main_object, str(test_object)
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
                    "ITS_STATION_FOUNDATION_COMPILE=FAIL\n"
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
                    "ITS_STATION_FOUNDATION_LINK=FAIL\n"
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
            env["V2X_PHY_AUDIT"] = "1"

            run = subprocess.run(
                [str(test_binary)],
                cwd=temp,
                env=env,
                text=True,
                capture_output=True,
                timeout=35,
            )
            output = run.stdout + run.stderr
            print(f"ITS_STATION_FOUNDATION_RUNTIME_RC={run.returncode}")
            for line in output.splitlines():
                if (
                    "ITS_STATION_" in line
                    or "STATION_ENDPOINT_REGISTRY=" in line
                    or "NS3_ACCESS_ADAPTER_" in line
                    or "[PHY_AUDIT] event=TRACE_CONNECT" in line
                ):
                    print(line)

            self.assertEqual(
                run.returncode,
                0,
                "Harness ITS Station falhou:\n"
                + "\n".join(output.splitlines()[-100:]),
            )

            for marker in (
                "ITS_STATION_STATE=PASS",
                "ITS_STATION_ADDRESS=PASS",
                "STATION_ENDPOINT_REGISTRY=PASS",
                "ITS_STATION_NETWORK=PASS",
                "NS3_ACCESS_ADAPTER_GENERIC=PASS",
                "ITS_STATION_FOUNDATION=PASS",
            ):
                self.assertIn(marker, output, f"Evidencia ausente: {marker}")

            self.assertRegex(
                output,
                r"\[PHY_AUDIT\] event=TRACE_CONNECT.*role=RSU",
                "O builder nao registou o papel PHY RSU",
            )
            self.assertRegex(
                output,
                r"\[PHY_AUDIT\] event=TRACE_CONNECT.*role=VRU",
                "O builder nao registou o papel PHY VRU",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
