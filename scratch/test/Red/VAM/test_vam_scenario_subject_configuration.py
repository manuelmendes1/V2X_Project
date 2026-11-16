#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]

HEADER = (
    ROOT / "scratch/vanetza_integration/context/"
    "simulation_controller/simulation_controller.h"
)
CONTROLLER = (
    ROOT / "scratch/vanetza_integration/context/"
    "simulation_controller/simulation_controller.cc"
)
MAIN = ROOT / "scratch/v2x/v2x_sumo.cc"


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


class TestVamScenarioSubjectConfiguration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text()
        cls.controller = CONTROLLER.read_text()
        cls.main = MAIN.read_text()

    def test_scenario_subject_config_is_explicit_and_typed(self):
        self.assertIn("struct VamVruScenarioSubjectConfig", self.header)
        for field in (
            "std::string personId",
            "std::uint32_t stationId",
            "bool activateRole",
            "VbsState vbsState",
        ):
            self.assertIn(field, self.header)

        self.assertIn(
            "ConfigureVamVruScenarioSubjects(",
            self.header,
        )

    def test_controller_owns_scenario_subject_configuration(self):
        self.assertIn(
            "m_vamVruScenarioSubjects",
            self.header,
        )
        self.assertIn(
            "SimulationController::m_vamVruScenarioSubjects",
            self.controller,
        )

    def test_configuration_validates_and_reserves_atomically(self):
        method = braced_block_after(
            self.controller,
            "SimulationController::ConfigureVamVruScenarioSubjects(",
        )
        self.assertTrue(method, "VAM_SCENARIO_CONFIGURATION_METHOD_MISSING")

        for token in (
            "m_vamGenerationScheduler",
            "subjects.empty()",
            "subject.personId.empty()",
            "subject.stationId == 0",
            "std::set<std::string>",
            "std::set<std::uint32_t>",
            "VbsState::Idle",
            "ItsStationIdentityAuthority::Instance()",
            "FindStationId",
            "RegisterVamVruStationIdentity",
            "Release(",
            "m_vamVruScenarioSubjects.swap",
        ):
            self.assertIn(token, method, f"VAM_SCENARIO_VALIDATION_MISSING: {token}")

    def test_empty_configuration_is_valid_without_scheduler(self):
        method = braced_block_after(
            self.controller,
            "SimulationController::ConfigureVamVruScenarioSubjects(",
        )
        self.assertTrue(method)
        empty_position = method.find("subjects.empty()")
        scheduler_position = method.find("m_vamGenerationScheduler")

        self.assertGreaterEqual(empty_position, 0)
        self.assertGreater(scheduler_position, empty_position)

    def test_subject_configuration_is_applied_through_public_role_api(self):
        method = braced_block_after(
            self.controller,
            "SimulationController::ApplyVamVruScenarioSubjectConfiguration(",
        )
        self.assertTrue(method, "VAM_SCENARIO_SUBJECT_APPLY_METHOD_MISSING")
        self.assertIn("m_vamVruScenarioSubjects.find(personId)", method)
        self.assertIn("ActivateVruSubject(personId)", method)
        self.assertIn("SetVruSubjectVbsState(", method)
        self.assertIn("VruTransitionResult::Rejected", method)

    def test_only_first_accepted_upsert_applies_scenario_configuration(self):
        handler = braced_block_after(
            self.controller,
            "SimulationController::HandlePersonUpdateFromSumo(",
        )
        self.assertTrue(handler, "SUMO_PERSON_HANDLER_MISSING")

        previous_position = handler.find(
            "m_sumoPersonRegistry.Find(datagram.personId)"
        )
        apply_position = handler.find(
            "m_sumoPersonRegistry.Apply(datagram)"
        )
        sync_position = handler.find(
            "SumoVruSubjectSynchronizer::SynchronizeAccepted"
        )
        scenario_position = handler.find(
            "ApplyVamVruScenarioSubjectConfiguration(datagram.personId)"
        )

        self.assertGreaterEqual(previous_position, 0)
        self.assertGreater(apply_position, previous_position)
        self.assertGreater(sync_position, apply_position)
        self.assertGreater(scenario_position, sync_position)
        self.assertIn("startsNewPresence", handler)
        self.assertIn("SumoPersonEventType::Upsert", handler)

    def test_scenario_configuration_is_applied_before_optional_audit(self):
        handler = braced_block_after(
            self.controller,
            "SimulationController::HandlePersonUpdateFromSumo(",
        )
        scenario_position = handler.find(
            "ApplyVamVruScenarioSubjectConfiguration(datagram.personId)"
        )
        audit_position = handler.find("V2X_SUMO_PERSON_AUDIT")

        self.assertGreaterEqual(scenario_position, 0)
        self.assertGreater(audit_position, scenario_position)

    def test_main_declares_subjects_without_invented_identity_values(self):
        self.assertIn(
            "std::vector<VamVruScenarioSubjectConfig> vamScenarioSubjects",
            self.main,
        )
        self.assertIn(
            "ConfigureVamVruScenarioSubjects(vamScenarioSubjects",
            self.main,
        )

        self.assertNotIn(
            '{"person-',
            self.main,
        )

    def test_subject_configuration_precedes_sumo_initialization(self):
        configure_position = self.main.find(
            "ConfigureVamVruScenarioSubjects(vamScenarioSubjects"
        )
        sumo_init_position = self.main.find("SumoAdapter::Init(")

        self.assertGreaterEqual(configure_position, 0)
        self.assertGreater(sumo_init_position, configure_position)


if __name__ == "__main__":
    unittest.main(verbosity=2)
