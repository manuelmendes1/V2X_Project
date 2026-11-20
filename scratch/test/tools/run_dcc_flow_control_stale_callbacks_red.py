from pathlib import Path
import os
import re
import subprocess
import tempfile
import sys


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (candidate / "ns3").is_file() and (
            candidate / "scratch" / "CMakeLists.txt"
        ).is_file():
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada")


def fail(label: str, result: subprocess.CompletedProcess) -> None:
    print(f"{label}=FAIL")
    print("STDOUT:")
    print(result.stdout[-8000:])
    print("STDERR:")
    print(result.stderr[-16000:])
    raise SystemExit(1)


def main() -> int:
    root = repository_root(Path(__file__))
    main_source = root / "scratch/v2x/v2x_sumo.cc"
    main_executable = root / "build/scratch/ns3.47-v2x_sumo-default"
    main_object = (
        "scratch/CMakeFiles/scratch_v2x_sumo.dir/v2x/v2x_sumo.cc.o"
    )
    test_source = (
        root
        / "scratch/vanetza_integration/"
          "test_dcc_flow_control_stale_callbacks.cc"
    )

    if not main_executable.is_file():
        print("PRODUCTION_EXECUTABLE_MISSING")
        return 1

    if not test_source.is_file():
        print("HARNESS_SOURCE_MISSING")
        return 1

    recipes = subprocess.run(
        [
            "ninja", "-C", "cmake-cache", "-t", "commands",
            "ns3.47-v2x_sumo-default",
        ],
        cwd=root,
        text=True,
        capture_output=True,
        timeout=45,
    )
    if recipes.returncode != 0:
        fail("NINJA_RECIPES", recipes)

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

    if len(compile_commands) != 1:
        print(f"COMPILE_RECIPE_COUNT={len(compile_commands)}")
        return 1

    if len(link_commands) != 1:
        print(f"LINK_RECIPE_COUNT={len(link_commands)}")
        return 1

    with tempfile.TemporaryDirectory(
        prefix="dcc-stale-callbacks-"
    ) as temporary:
        temp = Path(temporary)
        test_object = temp / "test_dcc_flow_control_stale_callbacks.o"
        test_binary = temp / "test_dcc_flow_control_stale_callbacks"

        compile_command = compile_commands[0].replace(
            str(main_source), str(test_source)
        )
        compile_command = compile_command.replace(
            main_object + ".d", str(test_object) + ".d"
        ).replace(main_object, str(test_object))

        compiled = subprocess.run(
            compile_command,
            shell=True,
            executable="/bin/bash",
            cwd=root / "cmake-cache",
            text=True,
            capture_output=True,
            timeout=180,
        )
        if compiled.returncode != 0:
            fail("DCC_STALE_CALLBACK_COMPILE", compiled)

        print("DCC_STALE_CALLBACK_COMPILE=PASS")

        link_command = link_commands[0].replace(
            str(main_executable), str(test_binary)
        ).replace(main_object, str(test_object))

        linked = subprocess.run(
            link_command,
            shell=True,
            executable="/bin/bash",
            cwd=root / "cmake-cache",
            text=True,
            capture_output=True,
            timeout=180,
        )
        if linked.returncode != 0:
            fail("DCC_STALE_CALLBACK_LINK", linked)

        print("DCC_STALE_CALLBACK_LINK=PASS")

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
        print("===== HARNESS OUTPUT =====")
        print(output, end="" if output.endswith("\n") else "\n")
        print(f"HARNESS_EXIT_CODE={run.returncode}")

        expected_red = [
            "PASS INITIAL_PACKET_TRANSMITTED_IMMEDIATELY",
            "PASS EXPIRED_PACKET_A_DROPPED",
            "FAIL ONLY_ONE_QUEUED_PACKET_TRANSMITTED_AT_100MS",
            "FAIL NEXT_QUEUED_PACKET_TRANSMITTED_AT_200MS",
            "OBS transmissions_at_100ms=3",
            "OBS transmissions_at_200ms=3",
            "OBS sent_at_ms=0,100,100",
            "DCC_FLOWCONTROL_STALE_CALLBACK=FAIL",
        ]

        reproduced = (
            run.returncode == 1
            and all(marker in output for marker in expected_red)
        )

        if reproduced:
            print("DCC_FLOWCONTROL_STALE_CALLBACK_RED_REPRODUCED=PASS")
            return 0

        print("DCC_FLOWCONTROL_STALE_CALLBACK_RED_REPRODUCED=NOT_CONFIRMED")
        print(
            "Interpreta o output do harness: a ausencia do padrao RED "
            "pode significar comportamento diferente ou um problema "
            "na reproducao."
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
