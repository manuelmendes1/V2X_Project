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


def extract_block(source: str, marker: str) -> str:
    marker_pos = source.find(marker)
    if marker_pos < 0:
        raise AssertionError(f"Bloco não encontrado: {marker}")

    open_pos = source.find("{", marker_pos)
    if open_pos < 0:
        raise AssertionError(f"Bloco sem '{{': {marker}")

    depth = 0
    for pos in range(open_pos, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[open_pos + 1:pos]

    raise AssertionError(f"Bloco não terminado: {marker}")


class TestVehicleV3VerifyDispatch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = repository_root(Path(__file__))
        path = (
            root
            / "scratch/vanetza_integration/wrappers/vanetza/security/"
            "denm_v3_verify_service.cc"
        )
        cls.source = path.read_text(encoding="utf-8")
        start = cls.source.index("DenmV3VerifyService::verify(")
        end = cls.source.index("\n} // namespace ns3", start)
        cls.verify_method = cls.source[start:end]

    def test_cam_aid36_has_explicit_strict_pki_route(self):
        body = extract_block(
            self.verify_method,
            "if (aid == 36 && m_impl->vehiclePkiConfigured)",
        )
        self.assertIn("m_impl->realVerifier->verify(request)", body)
        self.assertIn("RxBridgeContext::Current()", body)
        self.assertIn("ctx->verificationPerformed = true", body)
        self.assertIn("ctx->verifiedItsAid", body)
        self.assertIn("CAM_V3_VERIFY", body)

    def test_cam_aid36_legacy_fallback_is_explicitly_unconfigured_only(self):
        body = extract_block(
            self.verify_method,
            "if (aid == 36 && !m_impl->vehiclePkiConfigured)",
        )
        self.assertIn("m_impl->fallbackVerifier->verify(request)", body)

    def test_denm_aid37_keeps_real_v3_verification(self):
        body = extract_block(self.verify_method, "if (aid == 37)")
        self.assertIn("m_impl->realVerifier->verify(request)", body)
        self.assertIn("ctx->verifiedItsAid", body)
        self.assertIn("DENM_V3_VERIFY", body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
