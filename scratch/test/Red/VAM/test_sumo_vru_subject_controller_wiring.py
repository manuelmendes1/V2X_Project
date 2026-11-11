#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]

HEADER = (
    ROOT
    / "scratch/vanetza_integration/context/"
      "simulation_controller/simulation_controller.h"
)
CONTROLLER = (
    ROOT
    / "scratch/vanetza_integration/context/"
      "simulation_controller/simulation_controller.cc"
)
CMAKE = ROOT / "scratch/CMakeLists.txt"


def braced_block_after(source: str, marker: str) -> str:
    start = source.find(marker)
    if start < 0:
        return ""

    opening = source.find("{", start)
    if opening < 0:
        return ""

    depth = 0

    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]

    return ""


class TestSumoVruSubjectControllerWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text()
        cls.controller = CONTROLLER.read_text()
        cls.cmake = CMAKE.read_text()

    def test_controller_exposes_read_only_subject_registry(self):
        self.assertIn(
            '#include "vanetza_integration/context/'
            'sumo_vru_subject_registry/sumo_vru_subject_registry.h"',
            self.header,
        )
        self.assertIn(
            "static const SumoVruSubjectRegistry& "
            "GetSumoVruSubjectRegistry()",
            self.header,
        )

    def test_controller_exposes_explicit_role_management_operations(self):
        for signature in (
            "ActivateVruSubject(",
            "DeactivateVruSubject(",
            "SetVruSubjectVbsState(",
        ):
            self.assertIn(
                signature,
                self.header,
                f"API explícita em falta: {signature}",
            )

    def test_controller_owns_a_dedicated_subject_registry(self):
        self.assertIn(
            "static SumoVruSubjectRegistry m_sumoVruSubjectRegistry",
            self.header,
        )
        self.assertIn(
            "SumoVruSubjectRegistry SimulationController::"
            "m_sumoVruSubjectRegistry",
            self.controller,
        )

    def test_start_clears_person_and_subject_registries(self):
        start = braced_block_after(
            self.controller,
            "SimulationController::Start(Time stepLength)",
        )

        self.assertTrue(start, "SimulationController::Start não encontrado")
        self.assertIn("m_sumoPersonRegistry.Clear()", start)
        self.assertIn("m_sumoVruSubjectRegistry.Clear()", start)

    def test_rejected_person_event_cannot_update_subject_registry(self):
        handler = braced_block_after(
            self.controller,
            "SimulationController::HandlePersonUpdateFromSumo(",
        )

        self.assertTrue(
            handler,
            "HandlePersonUpdateFromSumo não implementado",
        )

        rejection = braced_block_after(
            handler,
            "if (!m_sumoPersonRegistry.Apply(datagram))",
        )

        self.assertTrue(
            rejection,
            "Handler must gate subject synchronization on PersonRegistry.Apply",
        )

        self.assertIn("return;", rejection)
        self.assertNotIn("m_sumoVruSubjectRegistry", rejection)

        person_apply_position = handler.find(
            "m_sumoPersonRegistry.Apply(datagram)"
        )
        sync_position = handler.find(
            "SumoVruSubjectSynchronizer::SynchronizeAccepted"
        )

        self.assertGreater(person_apply_position, -1)
        self.assertGreater(sync_position, person_apply_position)
        self.assertNotIn("SumoVruProfileMapper", handler)

    def test_controller_uses_profile_mapper_for_accepted_upserts(self):
        handler = braced_block_after(
            self.controller,
            "SimulationController::HandlePersonUpdateFromSumo(",
        )

        self.assertIn(
            "SumoVruSubjectSynchronizer::SynchronizeAccepted",
            handler,
        )

        synchronizer = (
            ROOT
            / "scratch/vanetza_integration/context/"
              "sumo_person_vru_subject_synchronizer/"
              "sumo_person_vru_subject_synchronizer.cc"
        ).read_text()

        self.assertIn(
            "SumoVruProfileMapper::Map(",
            synchronizer,
        )
        self.assertIn(
            "subjects.Upsert(datagram.personId, profile)",
            synchronizer,
        )

    def test_controller_removes_subject_on_accepted_remove(self):
        handler = braced_block_after(
            self.controller,
            "SimulationController::HandlePersonUpdateFromSumo(",
        )

        synchronizer = (
            ROOT
            / "scratch/vanetza_integration/context/"
              "sumo_person_vru_subject_synchronizer/"
              "sumo_person_vru_subject_synchronizer.cc"
        ).read_text()

        self.assertIn(
            "subjects.Remove(datagram.personId)",
            synchronizer,
        )

    def test_role_state_machine_and_policy_are_in_production_build(self):
        for source in (
            "vru/role/state_machine/vru_role_state_machine.cc",
            "vru/role/policy/vru_role_policy.cc",
            "context/sumo_vru_subject_registry/"
            "sumo_vru_subject_registry.cc",
            "context/sumo_person_vru_subject_synchronizer/"
            "sumo_person_vru_subject_synchronizer.cc",
        ):
            self.assertIn(
                source,
                self.cmake,
                f"Source não integrado no CMake de produção: {source}",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
