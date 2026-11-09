#!/usr/bin/env python3

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


HARNESS_TEMPLATE = r'''#include "__HEADER_INCLUDE__"
#include <iostream>
#include <optional>
#include <string>

int main()
{
    bool passed = true;
    const auto check = [&passed](bool condition, const char* label) {
        std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
        passed = passed && condition;
    };

    const auto legacy = ns3::ParseSumoPersonDatagram(
        "SUMO_PERSON,UPSERT,ped-legacy,1,2,1.2,90,1000");
    check(legacy.has_value(), "LEGACY_EIGHT_FIELD_UPSERT_ACCEPTED");
    check(legacy.has_value() && !legacy->sumoVClass.has_value(),
          "LEGACY_UPSERT_WITHOUT_VCLASS_REMAINS_UNKNOWN");

    const auto pedestrian = ns3::ParseSumoPersonDatagram(
        "SUMO_PERSON,UPSERT,ped-0,1,2,1.2,90,2000,pedestrian");
    check(pedestrian.has_value(), "NINE_FIELD_UPSERT_ACCEPTED");
    check(pedestrian.has_value() && pedestrian->sumoVClass == "pedestrian",
          "PEDESTRIAN_VCLASS_PRESERVED");

    const auto bicycle = ns3::ParseSumoPersonDatagram(
        "SUMO_PERSON,UPSERT,bike-0,1,2,1.2,90,3000,bicycle");
    check(bicycle.has_value() && bicycle->sumoVClass == "bicycle",
          "NON_PEDESTRIAN_VCLASS_PRESERVED_AS_METADATA");

    check(!ns3::ParseSumoPersonDatagram(
              "SUMO_PERSON,UPSERT,ped-0,1,2,1.2,90,4000,pedestrian\nINJECTED"),
          "VCLASS_LINE_BREAK_REJECTED");
    check(!ns3::ParseSumoPersonDatagram(
              "SUMO_PERSON,UPSERT,ped-0,1,2,1.2,90,5000,pedestrian,extra"),
          "VCLASS_EXTRA_FIELD_REJECTED");

    std::cout << "SUMO_PERSON_VCLASS_PARSER=" << (passed ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}
'''


def repository_root(start: Path) -> Path:
    resolved = start.resolve()
    for candidate in (resolved, *resolved.parents):
        if (
            (candidate / "scratch" / "test" / "run_tests.py").is_file()
            and (candidate / "scratch" / "vanetza_integration").is_dir()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestSumoPersonVClassParser(unittest.TestCase):
    def test_legacy_and_extended_person_upserts(self):
        root = repository_root(Path(__file__))
        existing_test = root / "scratch/test/Red/SUMO/test_sumo_person_datagram.cc"
        self.assertTrue(existing_test.is_file(), "Teste C++ existente do parser não encontrado")

        include_match = re.search(
            r'^\s*#include\s+"([^"\n]*sumo_person_datagram\.h)"',
            existing_test.read_text(),
            re.MULTILINE,
        )
        self.assertIsNotNone(include_match, "Include do header do parser não encontrado")
        include_path = include_match.group(1)
        header = root / "scratch" / include_path
        self.assertTrue(header.is_file(), f"Header do parser não encontrado: {header}")

        implementations = list(
            (root / "scratch").rglob("sumo_person_datagram.cc")
        )
        implementations = [p for p in implementations if "/test/" not in p.as_posix()]
        self.assertEqual(len(implementations), 1, "Implementação do parser ambígua ou ausente")

        compiler = shutil.which("g++-11") or shutil.which("g++")
        self.assertIsNotNone(compiler, "g++-11/g++ não encontrado")

        with tempfile.TemporaryDirectory(prefix="sumo-person-vclass-") as temp:
            temp_dir = Path(temp)
            source = temp_dir / "test_sumo_person_vclass.cc"
            source.write_text(HARNESS_TEMPLATE.replace("__HEADER_INCLUDE__", include_path))
            binary = temp_dir / "test_sumo_person_vclass"
            command = [
                compiler,
                "-std=c++23",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                f"-I{root / 'scratch'}",
                str(source),
                str(implementations[0]),
                "-o",
                str(binary),
            ]
            build = subprocess.run(
                command, cwd=root, capture_output=True, text=True, timeout=60
            )
            self.assertEqual(
                build.returncode,
                0,
                "SUMO_PERSON_VCLASS_PARSER_BUILD=FAIL\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{build.stdout}\nSTDERR:\n{build.stderr}",
            )
            print("SUMO_PERSON_VCLASS_PARSER_BUILD=PASS")

            run = subprocess.run(
                [str(binary)], cwd=root, capture_output=True, text=True, timeout=20
            )
            output = run.stdout + run.stderr
            print(output, end="")
            self.assertEqual(run.returncode, 0, output)
            self.assertIn("SUMO_PERSON_VCLASS_PARSER=PASS", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
