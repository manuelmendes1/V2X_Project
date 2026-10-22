#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
from pathlib import Path


def find_repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


def main():
    root = find_repository_root(Path(__file__))
    header = (
        root
        / "scratch/vanetza_integration/sumo_adapter/"
          "sumo_vehicle_datagram.h"
    )
    source = (
        root
        / "scratch/test/Green/CPM/"
          "test_sumo_vehicle_datagram_contract.cc"
    )

    if not header.is_file():
        print(
            "RED_EXPECTED: sumo_vehicle_datagram.h ainda não existe; "
            "o contrato ainda não foi implementado."
        )
        return 2

    compiler = shutil.which("g++-11") or shutil.which("g++")
    if not compiler:
        print("ERRO: não foi encontrado g++-11 nem g++.")
        return 3

    with tempfile.TemporaryDirectory(
        prefix="sumo-vehicle-datagram-"
    ) as temp:
        binary = Path(temp) / "test_sumo_vehicle_datagram_contract"

        command = [
            compiler,
            "-std=c++23",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            f"-I{root / 'scratch'}",
            str(source),
            str(
                root
                / "scratch/vanetza_integration/sumo_adapter/"
                  "sumo_vehicle_datagram.cc"
            ),
            "-o",
            str(binary),
        ]

        build = subprocess.run(
            command,
            cwd=root,
            text=True,
            check=False,
        )
        if build.returncode != 0:
            print("SUMO_VEHICLE_DATAGRAM_BUILD=FAIL")
            return build.returncode

        print("SUMO_VEHICLE_DATAGRAM_BUILD=PASS")

        result = subprocess.run(
            [str(binary)],
            cwd=root,
            text=True,
            check=False,
        )
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
