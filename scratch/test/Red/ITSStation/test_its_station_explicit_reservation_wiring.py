from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[4]


def read(relative_path):
    return (ROOT / relative_path).read_text(encoding="utf-8")


def extract_function(source, signature):
    start = source.find(signature)
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
                return source[start:index + 1]

    return ""


class TestItsStationExplicitReservationWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.controller = read(
            "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        )
        cls.cpm_endpoint = read(
            "scratch/vanetza_integration/cpm/emitter/"
            "cpm_emitter_endpoint.cc"
        )
        cls.spatem_endpoint = read(
            "scratch/vanetza_integration/spatem/emitter/"
            "spatem_emitter_endpoint.cc"
        )

        cls.cpm_config = extract_function(
            cls.controller,
            "SimulationController::ConfigureCpmPerception("
        )
        cls.spatem_config = extract_function(
            cls.controller,
            "SimulationController::ConfigureSpatemEmitter("
        )
        cls.cpm_create = extract_function(
            cls.cpm_endpoint,
            "CpmEmitterEndpoint::Create("
        )
        cls.spatem_create = extract_function(
            cls.spatem_endpoint,
            "SpatemEmitterEndpoint::Create("
        )

    def test_controller_and_endpoints_include_shared_authority(self):
        include = (
            "context/its_station_identity/"
            "its_station_identity_authority.h"
        )

        for name, source in (
            ("controller", self.controller),
            ("CPM endpoint", self.cpm_endpoint),
            ("SPATEM endpoint", self.spatem_endpoint),
        ):
            with self.subTest(component=name):
                self.assertIn(
                    include,
                    source,
                    f"{name.upper().replace(' ', '_')}_AUTHORITY_INCLUDE_MISSING",
                )

    def test_cpm_controller_reserves_after_validation_before_activation(self):
        body = self.cpm_config
        self.assertTrue(body, "CPM_CONFIGURATION_FUNCTION_NOT_FOUND")

        parse = body.find("ItsStartTimeParser::ParseUtc")
        reserve = body.find(".Reserve(")
        activate = body.find("m_cpmStationId = stationId")

        self.assertGreaterEqual(
            parse, 0, "CPM_ITS_START_TIME_VALIDATION_NOT_FOUND"
        )
        self.assertGreaterEqual(
            reserve, 0, "CPM_EXPLICIT_STATION_ID_RESERVATION_MISSING"
        )
        self.assertGreater(
            reserve, parse,
            "CPM_RESERVATION_MUST_FOLLOW_ALL_CONFIGURATION_VALIDATION",
        )
        self.assertGreater(
            activate, reserve,
            "CPM_CONFIGURATION_ACTIVATED_BEFORE_ID_RESERVATION",
        )
        self.assertIn(
            "ItsStationIdentityOwnerKind::ItsStation",
            body,
            "CPM_OWNER_KIND_MUST_BE_ITS_STATION",
        )
        self.assertIn(
            '"cpm-rsu-"',
            body,
            "CPM_OWNER_LOGICAL_ID_MUST_MATCH_ENDPOINT",
        )
        self.assertIn(
            "std::to_string(stationId)",
            body,
            "CPM_OWNER_MUST_USE_CONFIGURED_STATION_ID",
        )
        self.assertIn(
            "return false;",
            body[reserve:reserve + 800],
            "CPM_RESERVATION_FAILURE_MUST_REJECT_CONFIGURATION",
        )

    def test_spatem_controller_reserves_before_configuration_mutation(self):
        body = self.spatem_config
        self.assertTrue(body, "SPATEM_CONFIGURATION_FUNCTION_NOT_FOUND")

        reserve = body.find(".Reserve(")
        activate = body.find("m_spatemEnabled = enabled")

        self.assertGreaterEqual(
            reserve, 0, "SPATEM_EXPLICIT_STATION_ID_RESERVATION_MISSING"
        )
        self.assertGreater(
            activate, reserve,
            "SPATEM_CONFIGURATION_MUTATED_BEFORE_ID_RESERVATION",
        )
        self.assertIn(
            "ItsStationIdentityOwnerKind::ItsStation",
            body,
            "SPATEM_OWNER_KIND_MUST_BE_ITS_STATION",
        )
        self.assertIn(
            '"spatem-rsu-"',
            body,
            "SPATEM_OWNER_LOGICAL_ID_MUST_MATCH_ENDPOINT",
        )
        self.assertIn(
            "trafficLightId",
            body,
            "SPATEM_OWNER_MUST_USE_TRAFFIC_LIGHT_ID",
        )
        self.assertGreaterEqual(
            body.count("NS_ABORT_MSG_IF"),
            2,
            "SPATEM_RESERVATION_FAILURE_MUST_ABORT_CONFIGURATION",
        )

    def test_cpm_direct_creation_reserves_before_constructing_endpoint(self):
        body = self.cpm_create
        self.assertTrue(body, "CPM_CREATE_FUNCTION_NOT_FOUND")

        validation = body.find("Validate(config)")
        reserve = body.find(".Reserve(")
        construct = body.find("new CpmEmitterEndpoint")

        self.assertGreaterEqual(
            reserve, 0, "CPM_DIRECT_CREATION_RESERVATION_MISSING"
        )
        self.assertGreater(
            reserve, validation,
            "CPM_RESERVATION_MUST_FOLLOW_CONFIG_VALIDATION",
        )
        self.assertGreater(
            construct, reserve,
            "CPM_ENDPOINT_CONSTRUCTED_BEFORE_RESERVATION",
        )
        self.assertIn("config.logicalId", body)
        self.assertIn("config.stationId", body)
        self.assertIn(
            "return nullptr;",
            body[reserve:],
            "CPM_CREATE_MUST_REJECT_RESERVATION_COLLISION",
        )

    def test_spatem_direct_creation_reserves_before_constructing_endpoint(self):
        body = self.spatem_create
        self.assertTrue(body, "SPATEM_CREATE_FUNCTION_NOT_FOUND")

        validation = body.find("Validate(config)")
        reserve = body.find(".Reserve(")
        construct = body.find("new SpatemEmitterEndpoint")

        self.assertGreaterEqual(
            reserve, 0, "SPATEM_DIRECT_CREATION_RESERVATION_MISSING"
        )
        self.assertGreater(
            reserve, validation,
            "SPATEM_RESERVATION_MUST_FOLLOW_CONFIG_VALIDATION",
        )
        self.assertGreater(
            construct, reserve,
            "SPATEM_ENDPOINT_CONSTRUCTED_BEFORE_RESERVATION",
        )
        self.assertIn("config.logicalId", body)
        self.assertIn("config.stationId", body)
        self.assertIn(
            "return nullptr;",
            body[reserve:],
            "SPATEM_CREATE_MUST_REJECT_RESERVATION_COLLISION",
        )

    def test_controller_owner_keys_match_endpoint_logical_ids(self):
        self.assertIn(
            '"cpm-rsu-" + std::to_string(stationId)',
            self.cpm_config,
            "CPM_CONTROLLER_AND_ENDPOINT_OWNER_KEYS_MUST_MATCH",
        )
        self.assertIn(
            '"cpm-rsu-" + std::to_string(m_cpmStationId)',
            self.controller,
            "CPM_ENDPOINT_LOGICAL_ID_CONSTRUCTION_NOT_FOUND",
        )
        self.assertIn(
            '"spatem-rsu-" + trafficLightId',
            self.spatem_config,
            "SPATEM_CONTROLLER_OWNER_KEY_MUST_USE_TRAFFIC_LIGHT_ID",
        )
        self.assertIn(
            '"spatem-rsu-" + m_spatemTrafficLightId',
            self.controller,
            "SPATEM_ENDPOINT_LOGICAL_ID_CONSTRUCTION_NOT_FOUND",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
