#!/usr/bin/env python3

from pathlib import Path
import re
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "scratch" / "test" / "run_tests.py").is_file()
            and (candidate / "scratch" / "vanetza_integration").is_dir()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestSumoPersonAdapterWiring(unittest.TestCase):
    def test_adapter_exposes_a_dedicated_person_callback(self):
        root = repository_root(Path(__file__))
        header = (
            root / "scratch/vanetza_integration/sumo_adapter/sumo_adapter.h"
        ).read_text(encoding="utf-8")
        source = (
            root / "scratch/vanetza_integration/sumo_adapter/sumo_adapter.cc"
        ).read_text(encoding="utf-8")

        self.assertIn("SumoPersonDatagram", header)
        self.assertRegex(header, r"using\s+PersonUpdateCallback\s*=")
        self.assertIn("SetPersonUpdateCallback", header)
        self.assertIn("m_personCallback", source)
        self.assertIn("SumoAdapter::SetPersonUpdateCallback", source)

    def test_person_records_are_consumed_before_vehicle_fallback(self):
        root = repository_root(Path(__file__))
        source = (
            root / "scratch/vanetza_integration/sumo_adapter/sumo_adapter.cc"
        ).read_text(encoding="utf-8")

        prefix = re.search(
            r'message\.rfind\(\s*"SUMO_PERSON,"\s*,\s*0\s*\)\s*==\s*0',
            source,
        )
        vehicle_fallback = source.find("ns3::SumoParser::Parse(buffer)")
        self.assertIsNotNone(prefix, "SUMO_PERSON discriminator não encontrado")
        self.assertGreater(vehicle_fallback, prefix.start())

        person_block = source[prefix.start():vehicle_fallback]
        self.assertIn("ParseSumoPersonDatagram(message)", person_block)
        self.assertIn("m_personCallback", person_block)
        self.assertRegex(
            person_block,
            r"if\s*\(!person\)\s*\{\s*continue;\s*\}",
            "Datagrama SUMO_PERSON inválido tem de ser descartado, nunca "
            "encaminhado para o parser de veículos",
        )
        self.assertIn("continue;", person_block)

    def test_person_callback_type_does_not_reuse_vehicle_state(self):
        root = repository_root(Path(__file__))
        header = (
            root / "scratch/vanetza_integration/sumo_adapter/sumo_adapter.h"
        ).read_text(encoding="utf-8")
        callback_line = next(
            (line for line in header.splitlines()
             if "PersonUpdateCallback" in line and "using" in line),
            "",
        )
        self.assertTrue(callback_line, "PersonUpdateCallback não declarado")
        self.assertIn("SumoPersonDatagram", callback_line)
        self.assertNotIn("VehicleState", callback_line)


if __name__ == "__main__":
    unittest.main(verbosity=2)
