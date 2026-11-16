#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]

CONTROLLER = (
    ROOT / "scratch/vanetza_integration/context/"
    "simulation_controller/simulation_controller.cc"
)
SCHEDULER = (
    ROOT / "scratch/vanetza_integration/vam/scheduler/"
    "vam_generation_scheduler.cc"
)
SCHEDULER_HEADER = (
    ROOT / "scratch/vanetza_integration/vam/scheduler/"
    "vam_generation_scheduler.h"
)
MANAGER = (
    ROOT / "scratch/vanetza_integration/vam/generation/"
    "vam_generation_manager.cc"
)
MANAGER_HEADER = (
    ROOT / "scratch/vanetza_integration/vam/generation/"
    "vam_generation_manager.h"
)
CONTROLLER_HEADER = (
    ROOT / "scratch/vanetza_integration/context/"
    "simulation_controller/simulation_controller.h"
)


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


class TestVamSchedulerReconfiguration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.controller = CONTROLLER.read_text()
        cls.scheduler = SCHEDULER.read_text()
        cls.scheduler_header = SCHEDULER_HEADER.read_text()
        cls.manager = MANAGER.read_text()
        cls.manager_header = MANAGER_HEADER.read_text()
        cls.controller_header = CONTROLLER_HEADER.read_text()

        cls.configure = braced_block_after(
            cls.controller,
            "SimulationController::ConfigureVamGeneration(",
        )

    def replacement_construction_position(self):
        match = re.search(
            r"auto\s+replacementScheduler\s*=\s*"
            r"std::make_unique\s*<\s*"
            r"vanetza_integration::vam::VamGenerationScheduler\s*>\s*\(",
            self.configure,
            re.DOTALL,
        )
        return None if match is None else match.start()

    def test_invalid_enabled_configuration_does_not_destroy_current_scheduler(self):
        self.assertTrue(self.configure)

        disabled_branch = braced_block_after(
            self.configure,
            "if (!enabled)",
        )
        self.assertTrue(disabled_branch)

        self.assertIn("m_vamGenerationScheduler.reset()", disabled_branch)

        before_disabled_branch = self.configure.split(
            "if (!enabled)", 1
        )[0]
        self.assertNotIn(
            "m_vamGenerationScheduler.reset()",
            before_disabled_branch,
        )

    def test_valid_replacement_is_constructed_before_current_scheduler_is_replaced(self):
        self.assertIn("replacementScheduler", self.configure)

        construction = self.replacement_construction_position()
        publication = self.configure.find(
            "m_vamGenerationScheduler.swap(replacementScheduler)"
        )

        self.assertIsNotNone(construction)
        self.assertGreater(publication, construction)

    def test_all_enabled_configuration_validation_precedes_scheduler_replacement(self):
        validation = self.configure.find(
            "IsValidVamGenerationConfig(generation)"
        )
        construction = self.replacement_construction_position()
        publication = self.configure.find(
            "m_vamGenerationScheduler.swap(replacementScheduler)"
        )

        self.assertGreaterEqual(validation, 0)
        self.assertIsNotNone(construction)
        self.assertGreater(construction, validation)
        self.assertGreater(publication, construction)

    def test_identity_resolution_uses_global_authority_after_reconfiguration(self):
        evaluate = braced_block_after(
            self.scheduler,
            "VamGenerationScheduler::EvaluateStep(",
        )
        self.assertTrue(evaluate)

        self.assertIn(
            "ItsStationIdentityAuthority::Instance()",
            evaluate,
        )
        self.assertIn("FindStationId(owner)", evaluate)
        self.assertIn("IsReservedBy(", evaluate)

        self.assertNotIn(
            "m_stationIdByPerson",
            self.scheduler_header,
        )

    def test_reconfiguration_does_not_mutate_scenario_subject_declarations(self):
        self.assertIn(
            "m_vamVruScenarioSubjects",
            self.controller_header,
        )

        for forbidden_mutation in (
            "m_vamVruScenarioSubjects.clear()",
            "m_vamVruScenarioSubjects.swap(",
            "m_vamVruScenarioSubjects =",
        ):
            self.assertNotIn(forbidden_mutation, self.configure)

        # Scheduler reconfiguration must not access scenario declarations.
        # Their lifecycle is owned by ConfigureVamVruScenarioSubjects().
        self.assertNotIn(
            "m_vamVruScenarioSubjects",
            self.configure,
        )

    def test_controller_reuses_generation_manager_validation_contract(self):
        self.assertIn(
            "IsValidVamGenerationConfig(generation)",
            self.configure,
        )
        self.assertIn(
            "bool IsValidVamGenerationConfig(",
            self.manager_header,
        )

        validator = braced_block_after(
            self.manager,
            "IsValidVamGenerationConfig(",
        )
        self.assertTrue(validator)
        self.assertIn("return IsValidConfig(config);", validator)

    def test_scheduler_registration_is_idempotent_through_identity_authority(self):
        register = braced_block_after(
            self.scheduler,
            "VamGenerationScheduler::RegisterSubjectStationIdentity(",
        )
        self.assertTrue(register)

        self.assertIn("authority.Reserve(owner, stationId.value)", register)
        self.assertNotIn("m_subjectRuntimeByPerson", register)


if __name__ == "__main__":
    unittest.main(verbosity=2)
