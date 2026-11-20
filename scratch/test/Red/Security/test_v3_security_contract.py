from pathlib import Path
import re
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "scratch" / "CMakeLists.txt").is_file()
            and (candidate / "scripts" / "problem-tests.sh").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositório do projeto V2X não encontrada")


def strip_cpp_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


class TestV2xV3SecurityContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.integration = cls.root / "scratch/vanetza_integration"
        cls.main = (cls.root / "scratch/v2x/v2x_sumo.cc").read_text(encoding="utf-8")

        def read(relative: str) -> str:
            return (cls.integration / relative).read_text(encoding="utf-8")

        cls.vehicle_signer = read("wrappers/vanetza/security/denm_v3_sign_service.cc")
        cls.vehicle_signer_header = read("wrappers/vanetza/security/denm_v3_sign_service.h")
        cls.vehicle_verifier = read("wrappers/vanetza/security/denm_v3_verify_service.cc")
        cls.dispatcher = read("wrappers/vanetza/security/v2x_v3_verify_service.cc")
        cls.profile_header = read("wrappers/vanetza/security/v2x_v3_security_profile.h")
        cls.spatem_signer = read("wrappers/vanetza/security/spatem_v3_sign_service.cc")
        cls.spatem_verifier = read("wrappers/vanetza/security/spatem_v3_verify_service.cc")
        cls.spatem_runtime = read("spatem/emitter/spatem_rsu_runtime.cc")
        cls.spatem_rx = read("spatem/rx/spatem_rx_pipeline.cc")
        cls.cam_verify = read("rx/rx_verify/rx_verify.cc")
        cls.cam_rx = read("rx/rx_pipeline.cc")
        cls.denm_rx = read("denm/rx/denm_rx_pipeline.cc")
        cls.vehicle_initializer = read("context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.cc")
        cls.cmake = (cls.root / "scratch/CMakeLists.txt").read_text(encoding="utf-8")
        cls.simulation_context = read("context/simulation_context/simulation_context.h")

    def test_no_dummy_security_service_in_production_code(self):
        offenders = []
        pattern = re.compile(
            r"Dummy(?:Sign|Verify)Service"
        )
        for path in self.integration.rglob("*"):
            if path.suffix not in {".cc", ".h", ".hpp"}:
                continue
            source = strip_cpp_comments(path.read_text(encoding="utf-8"))
            if pattern.search(source):
                offenders.append(str(path.relative_to(self.root)))
        self.assertEqual([], offenders, "dummy security service remains in production sources")

    def test_vehicle_signer_uses_real_v3_for_cam_and_denm(self):
        for token in (
            "StraightSignService",
            "request.its_aid == vanetza::aid::DEN",
            "static_cast<vanetza::ItsAid>(36)",
            "use_runtime",
            "use_position_provider",
            "use_location_checker",
            "use_trust_store",
            "use_issuer_lookup",
            "denm-root.cert",
            "denm-aa.cert",
            "No_Certificate",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.vehicle_signer)
        self.assertIn("TrustStore", self.vehicle_signer_header)
        self.assertIn("IssuerMemoryLookup", self.vehicle_signer_header)
        self.assertNotIn("DummySignService", strip_cpp_comments(self.vehicle_signer))
        self.assertNotIn("disable_location_checks", self.vehicle_signer)
        self.assertNotIn("m_camFallback", self.vehicle_signer)

    def test_vehicle_verifier_validates_chain_time_and_location(self):
        for token in (
            "denm-root.cert",
            "denm-aa.cert",
            "TrustStore",
            "trustStore.insert(root)",
            "IssuerMemoryLookup",
            "use_trust_store",
            "use_issuer_lookup",
            "use_runtime",
            "use_position_provider",
            "use_location_checker",
            "StraightVerifyService",
            "RxBridgeContext::Current",
            "VerificationReport::Success",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.vehicle_verifier)
        self.assertNotIn("disable_location_checks", self.vehicle_verifier)
        self.assertNotIn("DummyVerifyService", strip_cpp_comments(self.vehicle_verifier))

    def test_denm_rx_requires_v3_cryptographic_verification(self):
        for token in (
            "ctx.verifiedItsAid != 37",
            "!ctx.verificationPerformed",
            "!ctx.verificationSucceeded",
            "denm_v3_verification_unavailable",
            "denm_v3_verification_failed",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.denm_rx)
        self.assertNotIn("disable_location_checks", self.vehicle_verifier)

    def test_missing_vehicle_ticket_has_no_synthesized_v3_fallback(self):
        self.assertIn("DiscoverDenmCredentials", self.vehicle_initializer)
        self.assertIn("SetActiveDenmCredential(0)", self.vehicle_initializer)
        self.assertNotIn("GenerateEcdsaKeyAndCert", self.vehicle_initializer)
        adapter = (self.integration / "wrappers/vanetza/geonet/vanetza_geonet_adapter.cc").read_text(encoding="utf-8")
        self.assertIn("V2X_V3_CREDENTIAL_REQUIRED", adapter)

    def test_dispatcher_has_distinct_trust_domains_and_fails_closed(self):
        for token in (
            "DenmV3VerifyService",
            "SpatemV3VerifyService",
            "CpmV3VerifyService",
            "ClassifyV2xV3SecurityProfile",
            "V2xV3SecurityProfile::VehicleCamDenm",
            "V2xV3SecurityProfile::SpatemRsu",
            "V2xV3SecurityProfile::CpmRsu",
            "VerificationReport::Configuration_Problem",
            "unsupported_or_unconfigured_security_profile",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.dispatcher)
        self.assertIn("case 36:", self.profile_header)
        self.assertIn("case 37:", self.profile_header)
        self.assertIn("case 137:", self.profile_header)
        self.assertIn("case 639:", self.profile_header)
        self.assertIn("default:", self.profile_header)

    def test_spatem_uses_its_own_real_v3_pki(self):
        for token in (
            "spatem-root.cert",
            "spatem-aa.cert",
            "spatem-rsu-ticket-000.cert",
            "spatem-rsu-ticket-000.pem",
            "TrustStore",
            "trustStore.insert(root)",
            "IssuerMemoryLookup",
            "use_runtime",
            "use_position_provider",
            "use_location_checker",
            "StraightSignService",
            "kSpatemItsAid = 137",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.spatem_signer)
        for token in (
            "StraightVerifyService",
            "CertificateCache",
            "TrustStore",
            "trustStore.insert(root)",
            "use_runtime",
            "use_position_provider",
            "use_location_checker",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.spatem_verifier)
        self.assertNotIn("disable_location_checks", self.spatem_signer)
        self.assertNotIn("disable_location_checks", self.spatem_verifier)
        self.assertIn("SpatemV3SignService", self.spatem_runtime)
        self.assertIn("SpatemV3VerifyService", self.spatem_runtime)
        self.assertNotIn("DummySignService", strip_cpp_comments(self.spatem_runtime))

    def test_spatem_rx_requires_positive_cryptographic_verdict(self):
        for token in (
            "context.verificationPerformed",
            "context.verificationSucceeded",
            "context.verifiedItsAid != 137",
            "spatem_v3_verification_missing",
            "spatem_v3_verification_failed",
            "SPATEM_CRYPTO_VERIFY_FAIL",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.spatem_rx)
        self.assertLess(
            self.spatem_rx.index("context.verificationSucceeded"),
            self.spatem_rx.index("VanetzaSpatemCodec::Decode"),
        )

    def test_cam_rx_requires_v3_and_binds_semantic_station_identity(self):
        for token in (
            "ctx.verificationPerformed",
            "ctx.verificationSucceeded",
            "ctx.verifiedItsAid != 36",
            "CamSecurity::Verify",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.cam_verify)
        self.assertLess(
            self.cam_verify.index("ctx.verificationSucceeded"),
            self.cam_verify.index("CamSecurity::Verify"),
        )
        self.assertIn("message.stationId != ctx.senderStationId", self.cam_rx)
        self.assertIn("cam_sender_station_id_mismatch", self.cam_rx)

    def test_vehicle_and_spatem_pki_are_explicit_runtime_inputs(self):
        for token in (
            '"denmPkiDir"',
            '"spatemPkiDir"',
            "VEHICLE_PKIDIR_REQUIRED",
            "SPATEM_PKIDIR_REQUIRED",
            "SimulationContext::denmPkiDirectory = denmPkiDir",
            "SimulationContext::spatemPkiDirectory = spatemPkiDir",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.main)
        self.assertIn("denmPkiDirectory", self.simulation_context)
        self.assertIn("spatemPkiDirectory", self.simulation_context)

    def test_spatem_v3_sources_are_compiled(self):
        for token in (
            "spatem_v3_sign_service.cc",
            "spatem_v3_verify_service.cc",
            "add_library(vanetza_spatem_obj OBJECT",
        ):
            with self.subTest(required=token):
                self.assertIn(token, self.cmake)


if __name__ == "__main__":
    unittest.main(verbosity=2)
