from pathlib import Path
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestCpmSecurityWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.base = cls.root / "scratch/vanetza_integration"
        cls.cmake = (
            cls.root / "scratch/CMakeLists.txt"
        ).read_text(encoding="utf-8")
        cls.cpm_runtime = (
            cls.base / "cpm/emitter/cpm_rsu_runtime.cc"
        ).read_text(encoding="utf-8")
        cls.geonet_adapter = (
            cls.base / "wrappers/vanetza/geonet/vanetza_geonet_adapter.cc"
        ).read_text(encoding="utf-8")
        cls.main = (
            cls.root / "scratch/v2x/v2x_sumo.cc"
        ).read_text(encoding="utf-8")
        cls.simulation_context = (
            cls.base / "context/simulation_context/simulation_context.h"
        ).read_text(encoding="utf-8")

    def test_rsu_runtime_uses_real_cpm_signer(self):
        self.assertIn(
            '#include "vanetza_integration/wrappers/vanetza/security/cpm_v3_sign_service.h"',
            self.cpm_runtime,
        )
        self.assertIn(
            "std::make_unique<CpmV3SignService>",
            self.cpm_runtime,
        )
        self.assertNotIn("DummySignService", self.cpm_runtime)

    def test_rx_security_entity_uses_v2x_verifier_dispatcher(self):
        self.assertIn(
            '#include "vanetza_integration/wrappers/vanetza/security/v2x_v3_verify_service.h"',
            self.geonet_adapter,
        )
        self.assertIn(
            "std::make_unique<V2xV3VerifyService>",
            self.geonet_adapter,
        )

    def test_cpm_verifier_uses_real_signature_and_chain_validation(self):
        path = (
            self.base
            / "wrappers/vanetza/security/cpm_v3_verify_service.cc"
        )
        self.assertTrue(path.is_file(), "CPM_V3_VERIFY_SOURCE_MISSING")
        source = path.read_text(encoding="utf-8")

        for required in (
            "StraightVerifyService",
            "TrustStore",
            "IssuerMemoryLookup",
            "use_trust_store",
            "use_issuer_lookup",
            "use_runtime",
            "use_position_provider",
            "use_location_checker",
            "RxBridgeContext::Current",
            "VerificationReport::Success",
        ):
            with self.subTest(required=required):
                self.assertIn(required, source)

        self.assertNotIn("DummyVerifyService", source)

    def test_cpm_signer_uses_real_signing_and_chain_validation(self):
        path = (
            self.base
            / "wrappers/vanetza/security/cpm_v3_sign_service.cc"
        )
        self.assertTrue(path.is_file(), "CPM_V3_SIGN_SOURCE_MISSING")
        source = path.read_text(encoding="utf-8")

        for required in (
            "StraightSignService",
            "DefaultSignHeaderPolicy",
            "StaticCertificateProvider",
            "TrustStore",
            "IssuerMemoryLookup",
            "use_trust_store",
            "use_issuer_lookup",
            "use_runtime",
            "use_position_provider",
            "use_location_checker",
        ):
            with self.subTest(required=required):
                self.assertIn(required, source)

        self.assertNotIn("DummySignService", source)

    def test_cpm_signer_rejects_mismatched_ticket_private_key(self):
        source = (
            self.base
            / "wrappers/vanetza/security/cpm_v3_sign_service.cc"
        ).read_text(encoding="utf-8")
        self.assertIn("VehicleCredentialBindingValidator::Validate", source)
        self.assertIn(
            "CPM ticket certificate/private key binding validation failed",
            source,
        )

    def test_dispatcher_routes_cpm_and_preserves_denm_verification(self):
        path = (
            self.base
            / "wrappers/vanetza/security/v2x_v3_verify_service.cc"
        )
        self.assertTrue(path.is_file(), "V2X_V3_VERIFY_DISPATCHER_MISSING")
        source = path.read_text(encoding="utf-8")

        for required in (
            "CpmV3VerifyService",
            "DenmV3VerifyService",
            "ClassifyV2xV3SecurityProfile",
            "V2xV3SecurityProfile::CpmRsu",
            "VerificationReport::Configuration_Problem",
        ):
            with self.subTest(required=required):
                self.assertIn(required, source)

    def test_cpm_pki_argument_is_separate_from_denm_pki(self):
        self.assertIn("cpmPkiDir", self.main)
        self.assertIn('"cpmPkiDir"', self.main)
        self.assertIn(
            "SimulationContext::cpmPkiDirectory = cpmPkiDir",
            self.main,
        )
        self.assertIn("cpmPkiDirectory", self.simulation_context)

    def test_security_sources_are_in_existing_cpm_object_target(self):
        for required in (
            "cpm_v3_sign_service.cc",
            "cpm_v3_verify_service.cc",
            "v2x_v3_verify_service.cc",
            "add_library(vanetza_cpm_perception_obj OBJECT",
        ):
            with self.subTest(required=required):
                self.assertIn(required, self.cmake)

        self.assertNotIn("add_executable(cpm_security", self.cmake)
        self.assertIn(
            'create_scratch("${CMAKE_CURRENT_SOURCE_DIR}/v2x/v2x_sumo.cc")',
            self.cmake,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
