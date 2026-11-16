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


class TestVamGenerationSchedulerControllerWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text()
        cls.controller = CONTROLLER.read_text()
        cls.main = MAIN.read_text()

    def test_controller_exposes_explicit_vam_management_api(self):
        self.assertIn(
            "ConfigureVamGeneration(",
            self.header,
        )
        self.assertIn(
            "RegisterVamVruStationIdentity(",
            self.header,
        )
        self.assertIn(
            "GetPendingVamGenerationCandidates(",
            self.header,
        )
        self.assertIn("ConfirmVamGeneration(", self.header)
        self.assertIn("AbortVamGeneration(", self.header)

    def test_controller_owns_dedicated_scheduler(self):
        self.assertIn(
            '#include "vanetza_integration/vam/scheduler/'
            'vam_generation_scheduler.h"',
            self.header,
        )
        self.assertIn(
            "std::unique_ptr<"
            "vanetza_integration::vam::VamGenerationScheduler>",
            self.header,
        )
        self.assertIn(
            "SimulationController::m_vamGenerationScheduler",
            self.controller,
        )

    def test_start_resets_transient_scheduler_state(self):
        start = braced_block_after(
            self.controller,
            "SimulationController::Start(Time stepLength)",
        )
        self.assertTrue(start, "SIMULATION_CONTROLLER_START_MISSING")
        self.assertIn(
            "m_vamGenerationScheduler->ResetRuntime()",
            start,
        )

    def test_configuration_requires_its_time_and_explicit_thresholds(self):
        method = braced_block_after(
            self.controller,
            "SimulationController::ConfigureVamGeneration(",
        )
        self.assertTrue(method, "VAM_CONFIGURATION_METHOD_MISSING")
        self.assertIn("SimulationContext::itsStartTime", method)
        self.assertIn("ItsStartTimeParser::ParseUtc", method)
        self.assertIn("std::isfinite", method)
        self.assertIn("tCheckVamGenMs", method)
        self.assertIn("snapshotBuilder.vehicleThresholds", method)
        self.assertIn("snapshotBuilder.vruThresholds", method)
        self.assertIn("maximumSampleAgeUs", method)

    def test_person_identity_registration_delegates_to_scheduler(self):
        method = braced_block_after(
            self.controller,
            "SimulationController::RegisterVamVruStationIdentity(",
        )
        self.assertTrue(method, "VAM_IDENTITY_REGISTRATION_METHOD_MISSING")
        self.assertIn("m_vamGenerationScheduler", method)
        self.assertIn("VamStationId", method)

    def test_candidate_resolution_delegates_to_scheduler(self):
        confirm = braced_block_after(
            self.controller,
            "SimulationController::ConfirmVamGeneration(",
        )
        abort = braced_block_after(
            self.controller,
            "SimulationController::AbortVamGeneration(",
        )
        pending = braced_block_after(
            self.controller,
            "SimulationController::GetPendingVamGenerationCandidates(",
        )

        self.assertTrue(confirm, "VAM_CONFIRM_METHOD_MISSING")
        self.assertTrue(abort, "VAM_ABORT_METHOD_MISSING")
        self.assertTrue(pending, "VAM_PENDING_CANDIDATES_METHOD_MISSING")

        self.assertIn("ConfirmGeneration", confirm)
        self.assertIn("AbortGeneration", abort)
        self.assertIn("GetPendingCandidates", pending)

    def test_step_complete_invokes_vam_after_cpm(self):
        method = braced_block_after(
            self.controller,
            "SimulationController::HandleSumoStepComplete(",
        )
        self.assertTrue(method, "SUMO_STEP_COMPLETE_HANDLER_MISSING")

        cpm_position = method.find("RunCpmPerception(referenceTimeUs)")
        vam_position = method.find("RunVamGeneration(referenceTimeUs)")

        self.assertGreaterEqual(cpm_position, 0)
        self.assertGreater(vam_position, cpm_position)

    def test_vam_runtime_uses_sumo_time_and_does_not_transmit(self):
        method = braced_block_after(
            self.controller,
            "SimulationController::RunVamGeneration(",
        )
        self.assertTrue(method, "VAM_RUNTIME_METHOD_MISSING")
        self.assertIn("EvaluateStep(", method)
        self.assertIn("referenceTimeUs", method)
        self.assertIn("m_sumoPersonRegistry", method)
        self.assertIn("m_sumoVruSubjectRegistry", method)
        self.assertNotIn("Simulator::Now()", method)
        self.assertNotIn("SendVam(", method)
        self.assertNotIn("TransmitVam(", method)
        self.assertNotIn("ConfirmVamGeneration(", method)

        evaluated_position = method.find(
            '"[VAM_GENERATION] CANDIDATE_EVALUATED"'
        )
        abort_position = method.find("AbortVamGeneration(")
        aborted_position = method.find(
            '"[VAM_GENERATION] CANDIDATE_ABORTED"'
        )

        self.assertGreaterEqual(
            evaluated_position, 0,
            "EVALUATION_ONLY_CANDIDATE_LOG_MISSING",
        )
        self.assertGreater(
            abort_position, evaluated_position,
            "CANDIDATE_MUST_BE_ABORTED_AFTER_BEING_REPORTED",
        )
        self.assertGreater(
            aborted_position, abort_position,
            "CANDIDATE_ABORT_RESULT_LOG_MISSING",
        )

    def test_cli_keeps_vam_generation_opt_in(self):
        self.assertIn(
            "bool enableVamGeneration = false",
            self.main,
        )
        for option in (
            "enableVamGeneration",
            "vamTCheckVamGenMs",
            "vamVehicleSafeLateralMeters",
            "vamVehicleSafeLongitudinalMeters",
            "vamVehicleSafeVerticalMeters",
            "vamVruSafeLateralMeters",
            "vamVruSafeLongitudinalMeters",
            "vamVruSafeVerticalMeters",
            "vamMaximumSampleAgeUs",
        ):
            self.assertIn(
                f'"{option}"',
                self.main,
                f"VAM_CLI_OPTION_MISSING: {option}",
            )

        self.assertIn(
            "SimulationController::ConfigureVamGeneration(",
            self.main,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
