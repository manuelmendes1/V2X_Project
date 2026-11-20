from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (candidate / "ns3").is_file() and (
            candidate / "scratch" / "CMakeLists.txt"
        ).is_file():
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada")


class TestVamRealDccAccess(unittest.TestCase):
    def test_real_dcc_access_success_and_terminal_failure(self):
        root = repository_root(Path(__file__))
        main_source = root / "scratch/v2x/v2x_sumo.cc"
        main_executable = root / "build/scratch/ns3.47-v2x_sumo-default"
        main_object = "scratch/CMakeFiles/scratch_v2x_sumo.dir/v2x/v2x_sumo.cc.o"
        test_source = root / "scratch/test/Red/VAM/test_vam_real_dcc_access.cc"

        self.assertTrue(main_executable.is_file(), "Executavel de referencia em falta")
        self.assertTrue(test_source.is_file(), "Harness C++ em falta")

        recipes = subprocess.run(
            ["ninja", "-C", "cmake-cache", "-t", "commands", "ns3.47-v2x_sumo-default"],
            cwd=root,
            text=True,
            capture_output=True,
            timeout=45,
        )
        self.assertEqual(
            recipes.returncode,
            0,
            "Nao foi possivel obter as receitas Ninja:\n" + recipes.stderr,
        )
        commands = recipes.stdout.splitlines()
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
        self.assertEqual(len(compile_commands), 1, "Receita de compilacao nao unica")
        self.assertEqual(len(link_commands), 1, "Receita de link nao unica")

        with tempfile.TemporaryDirectory(prefix="vam-real-dcc-access-") as temporary:
            temp = Path(temporary)
            test_object = temp / "test_vam_real_dcc_access.cc.o"
            test_binary = temp / "test_vam_real_dcc_access"

            compile_command = compile_commands[0].replace(
                str(main_source), str(test_source)
            )
            compile_command = compile_command.replace(
                main_object + ".d", str(test_object) + ".d"
            ).replace(main_object, str(test_object))

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
                    "VAM_REAL_DCC_ACCESS_COMPILE=FAIL\n"
                    f"STDOUT:\n{compile_result.stdout[-6000:]}\n"
                    f"STDERR:\n{compile_result.stderr[-18000:]}"
                )

            link_command = link_commands[0].replace(
                str(main_executable), str(test_binary)
            ).replace(main_object, str(test_object))
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
                    "VAM_REAL_DCC_ACCESS_LINK=FAIL\n"
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
                timeout=45,
            )
            output = run.stdout + run.stderr
            print("VAM_REAL_DCC_ACCESS_BUILD=PASS")
            print(output, end="")
            self.assertEqual(
                run.returncode,
                0,
                f"DCC/Access real falhou, RC={run.returncode}:\n{output[-12000:]}",
            )
            self.assertIn("VAM_REAL_DCC_ACCESS=PASS", output)

            shb_lines = re.findall(
                r"^\[GEONET_SHB_TX\].*$",
                output,
                re.MULTILINE,
            )

            self.assertGreaterEqual(
                len(shb_lines),
                2,
                "Esperava pelo menos dois SHB serializados "
                "(cenário imediato e cenário temporal):\n" + output,
            )

            timestamp_match = re.search(
                r"\btimestamp_raw=(\d+)\b",
                shb_lines[-1],
            )

            self.assertIsNotNone(
                timestamp_match,
                "O último SHB não expõe timestamp_raw:\n" + shb_lines[-1],
            )

            actual_timestamp = int(timestamp_match.group(1))

            self.assertEqual(
                actual_timestamp,
                5025,
                "Timestamp SHB incorreto: "
                f"expected=5025 actual={actual_timestamp}\n"
                f"SHB observado: {shb_lines[-1]}",
            )

            print("VAM_RSU_SHB_TIMESTAMP=PASS")



if __name__ == "__main__":
    unittest.main(verbosity=2)
