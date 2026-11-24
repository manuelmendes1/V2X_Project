from pathlib import Path
import re
import unittest


def repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / ".git").exists()
            and (candidate / "scratch/CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("V2X repository root not found")


def strip_cpp_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def function_body(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0

    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]

    raise AssertionError(f"Unclosed function body: {marker}")


class TestVehicleV3SigningContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.integration = cls.root / "scratch/vanetza_integration"

        def read(relative):
            return (
                cls.integration / relative
            ).read_text(encoding="utf-8")

        cls.signer = strip_cpp_comments(
            read("wrappers/vanetza/security/denm_v3_sign_service.cc")
        )
        cls.signer_header = strip_cpp_comments(
            read("wrappers/vanetza/security/denm_v3_sign_service.h")
        )
        cls.geonet_adapter = strip_cpp_comments(
            read("wrappers/vanetza/geonet/vanetza_geonet_adapter.cc")
        )

    def test_vehicle_signer_has_no_dummy_fallback(self):
        combined = self.signer + "\n" + self.signer_header

        self.assertNotRegex(
            combined,
            r"\bDummySignService\b",
            "Vehicle CAM/DENM signer still contains a dummy service",
        )
        self.assertNotIn(
            "m_camFallback",
            combined,
            "CAM must not use a separate dummy fallback",
        )

    def test_cam_and_denm_share_the_same_v3_signer(self):
        sign_body = function_body(
            self.signer,
            "DenmV3SignService::sign(",
        )

        self.assertIn("kCamItsAid", sign_body)
        self.assertIn("kDenmItsAid", sign_body)
        self.assertIn("m_denmSigner->sign", sign_body)
        self.assertNotIn("m_camFallback", sign_body)

    def test_active_ticket_is_authorized_for_cam_and_denm(self):
        reload_body = function_body(
            self.signer,
            "DenmV3SignService::ReloadActiveCredential(",
        )

        self.assertIn("GetActiveCredential()", reload_body)
        self.assertIn("m_materialResolver.Resolve(*credential)", reload_body)
        self.assertIn("valid_for_signing", reload_body)
        self.assertIn("kCamItsAid", reload_body)
        self.assertIn("kDenmItsAid", reload_body)

    def test_signer_uses_the_vehicle_run_trust_domain(self):
        self.assertIn("vehiclePkiRunDirectory", self.signer_header)
        self.assertIn("vehiclePkiRunDirectory", self.signer)

        for required in (
            "trust-store/trusted-root.cert",
            "certificate-chain/aa.cert",
            "certificate-chain/root.cert",
            "SameCertificateEncoding",
            "trustStore.insert",
            "issuerLookup.insert",
            "use_trust_store",
            "use_issuer_lookup",
            "use_runtime",
            "use_position_provider",
            "use_location_checker",
        ):
            with self.subTest(required=required):
                self.assertIn(required, self.signer)

        self.assertNotIn(
            "disable_location_checks(true)",
            self.signer,
            "Vehicle V3 signer must not disable geographic validation",
        )

    def test_geonet_adapter_never_constructs_a_dummy_signer(self):
        self.assertNotRegex(
            self.geonet_adapter,
            r"\bDummySignService\b",
        )
        self.assertNotIn(
            "signer=DUMMY",
            self.geonet_adapter,
        )

    def test_geonet_adapter_passes_the_vehicle_run_to_the_signer(self):
        self.assertIn("std::make_unique<DenmV3SignService>", self.geonet_adapter)
        self.assertIn("m_impl->vehicle.security.vehiclePkiRunDirectory", self.geonet_adapter)
        self.assertIn("security.credentialPool", self.geonet_adapter)


    def test_signer_requests_ticket_certificate_for_new_active_ticket(self):
        reload_body = function_body(
            self.signer,
            "DenmV3SignService::ReloadActiveCredential(",
        )
        request_index = reload_body.find("newSignHeaderPolicy->request_certificate()")
        signer_index = reload_body.find("StraightSignService")
        self.assertGreaterEqual(request_index, 0)
        self.assertGreater(signer_index, request_index)


if __name__ == "__main__":
    unittest.main(verbosity=2)
