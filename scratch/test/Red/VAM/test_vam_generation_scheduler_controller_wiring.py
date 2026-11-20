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
        self.assertIn("VamGenerationTransactionHandle", self.header)

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

    def test_evaluate_step_rejects_exhausted_scheduler_identity(self):
        scheduler = (
            ROOT / "scratch/vanetza_integration/vam/scheduler/"
            "vam_generation_scheduler.cc"
        ).read_text()
        evaluate = braced_block_after(
            scheduler,
            "VamGenerationScheduler::EvaluateStep(",
        )
        self.assertTrue(evaluate, "VAM_EVALUATE_STEP_MISSING")
        guard = evaluate.find(
            "HasValidVamGenerationSchedulerInstanceId("
        )
        rejection = evaluate.find(
            "VAM scheduler instance identifier is exhausted"
        )
        handle_creation = evaluate.find("VamGenerationTransactionHandle{")
        self.assertGreaterEqual(guard, 0, "EVALUATE_IDENTITY_GUARD_MISSING")
        self.assertGreater(rejection, guard)
        self.assertGreater(handle_creation, rejection)

    def test_start_does_not_reset_configured_vam_scheduler(self):
        start = braced_block_after(
            self.controller,
            "SimulationController::Start(Time stepLength)",
        )
        self.assertTrue(start, "SIMULATION_CONTROLLER_START_MISSING")
        self.assertNotIn(
            "ResetRuntime(",
            start,
            "START_MUST_NOT_DISCARD_VAM_TRANSACTION_STATE",
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
        self.assertIn("handle", confirm)
        self.assertIn("handle", abort)
        self.assertNotIn("personId", confirm)
        self.assertNotIn("transactionId", confirm)
        self.assertNotIn("personId", abort)
        self.assertNotIn("transactionId", abort)

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
        self.assertIn("candidate.transactionHandle", method)

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

    def test_scheduler_identity_exhaustion_preserves_active_instance(self):
        method = braced_block_after(
            self.controller,
            "SimulationController::ConfigureVamGeneration(",
        )
        self.assertTrue(method, "VAM_CONFIGURATION_METHOD_MISSING")
        guard = method.find("HasValidInstanceIdentity()")
        swap = method.find("m_vamGenerationScheduler.swap(replacementScheduler)")
        self.assertGreaterEqual(guard, 0, "SCHEDULER_IDENTITY_GUARD_MISSING")
        self.assertGreater(swap, guard, "INVALID_SCHEDULER_MUST_NOT_REPLACE_ACTIVE")

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


    def test_cli_loads_optional_sumo_network_projection_before_vam_configuration(self):
        self.assertIn(
            '#include "vanetza_integration/sumo_adapter/'
            'sumo_network_projection_loader.h"',
            self.main,
            "SUMO_PROJECTION_LOADER_INCLUDE_MISSING",
        )
        self.assertIn(
            "std::string sumoNetFile",
            self.main,
            "SUMO_NET_FILE_OPTION_STORAGE_MISSING",
        )
        self.assertIn(
            '"sumoNetFile"',
            self.main,
            "SUMO_NET_FILE_CLI_OPTION_MISSING",
        )
        self.assertIn(
            "SumoNetworkProjectionLoader::LoadUtmWgs84(",
            self.main,
            "SUMO_NETWORK_PROJECTION_LOAD_MISSING",
        )

        parse_position = self.main.find("cmd.Parse(argc, argv)")
        load_position = self.main.find(
            "SumoNetworkProjectionLoader::LoadUtmWgs84("
        )
        configure_position = self.main.find(
            "SimulationController::ConfigureVamGeneration("
        )

        self.assertGreaterEqual(parse_position, 0)
        self.assertGreater(load_position, parse_position)
        self.assertGreater(configure_position, load_position)

        optional_load = braced_block_after(
            self.main,
            "if (!sumoNetFile.empty())",
        )
        self.assertTrue(
            optional_load,
            "SUMO_NET_FILE_MUST_REMAIN_OPTIONAL",
        )
        self.assertIn("LoadUtmWgs84(", optional_load)
        self.assertIn("return 2;", optional_load)
        self.assertIn(
            "[SUMO_PROJECTION]",
            optional_load,
            "SUMO_PROJECTION_STARTUP_DIAGNOSTIC_MISSING",
        )

    def test_controller_forwards_optional_projection_to_scheduler(self):
        expected_parameter = (
            "std::optional<ns3::SumoUtmWgs84ProjectionConfig>"
            " sumoProjection = std::nullopt"
        )
        self.assertIn(
            expected_parameter,
            self.header,
            "CONTROLLER_OPTIONAL_PROJECTION_API_MISSING",
        )

        method = braced_block_after(
            self.controller,
            "SimulationController::ConfigureVamGeneration(",
        )
        self.assertTrue(method, "VAM_CONFIGURATION_METHOD_MISSING")
        self.assertIn(
            "config.sumoProjection",
            method,
            "VAM_SCHEDULER_PROJECTION_FORWARDING_MISSING",
        )
        self.assertIn(
            "sumoProjection",
            method,
            "VAM_SCHEDULER_PROJECTION_PARAMETER_UNUSED",
        )

        self.assertIn(
            "vamEvaluationConfig,\n"
            "            vamConfigurationError,\n"
            "            sumoProjection)",
            self.main,
            "CLI_PROJECTION_NOT_PASSED_TO_CONTROLLER",
        )

    def test_existing_controller_callers_remain_source_compatible(self):
        expected_default = (
            "std::optional<ns3::SumoUtmWgs84ProjectionConfig>"
            " sumoProjection = std::nullopt"
        )
        self.assertIn(
            expected_default,
            self.header,
            "OPTIONAL_PROJECTION_DEFAULT_MISSING",
        )

        # Existing test/runtime callers use the original four-argument API.
        self.assertIn(
            "true, invalid, evaluation, error)",
            self.controller
            + (
                ROOT / "scratch/test/Red/VAM/"
                "test_vam_scenario_subject_runtime_udp.cc"
            ).read_text(),
            "FOUR_ARGUMENT_CONTROLLER_CALLERS_MUST_REMAIN_VALID",
        )


    def test_production_candidate_is_encoded_before_transaction_abort(self):
        self.assertIn(
            '#include "vanetza_integration/vam/codec/vam_uper_encoder.h"',
            self.controller,
            "PRODUCTION_VAM_ENCODER_INCLUDE_MISSING",
        )

        method = braced_block_after(
            self.controller,
            "SimulationController::RunVamGeneration(",
        )
        self.assertTrue(method, "VAM_RUNTIME_METHOD_MISSING")

        encode_position = method.find("VamUperEncoder::Encode(")
        abort_position = method.find("AbortVamGeneration(")

        self.assertGreaterEqual(
            encode_position, 0,
            "PRODUCTION_CANDIDATE_MUST_REACH_UPER_ENCODER",
        )
        self.assertGreater(
            abort_position, encode_position,
            "EVALUATION_TRANSACTION_MUST_BE_ABORTED_AFTER_ENCODING",
        )
        self.assertNotIn(
            "ConfirmVamGeneration(",
            method,
            "EVALUATION_ONLY_MUST_NOT_CONFIRM_GENERATION",
        )
        self.assertIn("candidate.input", method)
        self.assertIn(
            "candidate.decision.includeLowFrequencyContainer",
            method,
            "LF_DECISION_MUST_BE_FORWARDED_FROM_PRODUCTION_CANDIDATE",
        )
        self.assertIn(
            "VamUperEncodeOptions",
            method,
            "PRODUCTION_ENCODER_OPTIONS_MISSING",
        )



if __name__ == "__main__":
    unittest.main(verbosity=2)
