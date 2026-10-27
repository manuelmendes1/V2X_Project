from pathlib import Path
import re
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositório ns-3 não encontrada")


class TestCpmSecurityFaultInjection(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = repository_root(Path(__file__))
        adapter_path = (
            root
            / "scratch/vanetza_integration/wrappers/vanetza/access/"
            "ns3_access_adapter.cc"
        )
        cls.source = adapter_path.read_text(encoding="utf-8")
        cls.cmake = (root / "scratch/CMakeLists.txt").read_text(
            encoding="utf-8"
        )

    def assert_hook_compile_gated(self, token: str):
        token_position = self.source.index(token)
        prefix = self.source[:token_position]

        guard_start = prefix.rfind(
            "#if defined(V2X_ENABLE_SECURITY_TEST_HOOKS)"
        )
        guard_end = prefix.rfind(
            "#endif // V2X_ENABLE_SECURITY_TEST_HOOKS"
        )

        self.assertGreater(
            guard_start,
            guard_end,
            f"{token!r} not guarded by V2X_ENABLE_SECURITY_TEST_HOOKS",
        )

    def test_cpm_tamper_hook_is_scoped_and_explicit(self):
        required = (
            'std::getenv("V2X_CPM_TAMPER")',
            "securedTxAid == 639",
            "[CPM_V3_TAMPER]",
            "target=network_last_byte",
            "frame[tamperOffset] ^= 0x01;",
        )
        for token in required:
            with self.subTest(required=token):
                self.assertIn(token, self.source)

        self.assertRegex(
            self.cmake,
            r'(?s)option\s*\(\s*V2X_ENABLE_SECURITY_TEST_HOOKS'
            r'\s*"[^"]+"\s+OFF\s*\)',
        )
        self.assertIn(
            "if(V2X_ENABLE_SECURITY_TEST_HOOKS)",
            self.cmake,
        )
        self.assertIn(
            "target_compile_definitions(vanetza_cam_encoder_obj "
            "PRIVATE V2X_ENABLE_SECURITY_TEST_HOOKS=1)",
            self.cmake,
        )
        self.assert_hook_compile_gated('std::getenv("V2X_CPM_TAMPER")')
        self.assert_hook_compile_gated("[CPM_V3_TAMPER]")

    def test_cpm_duplicate_hook_retransmits_exact_packet(self):
        required = (
            'std::getenv("V2X_CPM_DUPLICATE")',
            "securedTxAid == 639",
            "[CPM_V3_DUPLICATE]",
            "ns3Packet->Copy()",
            "MicroSeconds(2000)",
            "socket->SendTo(",
        )
        for token in required:
            with self.subTest(required=token):
                self.assertIn(token, self.source)

        self.assert_hook_compile_gated(
            'std::getenv("V2X_CPM_DUPLICATE")'
        )
        self.assert_hook_compile_gated("[CPM_V3_DUPLICATE]")

    def test_existing_denm_hooks_remain_available(self):
        required = (
            'std::getenv("V2X_DENM_TAMPER")',
            'std::getenv("V2X_DENM_DUPLICATE")',
            'std::getenv("V2X_SPATEM_DUPLICATE")',
            "securedTxAid == 37",
            "[DENM_V3_TAMPER]",
            "[DENM_V3_DUPLICATE]",
            "[SPATEM_PHYSICAL_DUPLICATE]",
        )
        for token in required:
            with self.subTest(required=token):
                self.assertIn(token, self.source)

        for token in (
            'std::getenv("V2X_DENM_TAMPER")',
            'std::getenv("V2X_DENM_DUPLICATE")',
            'std::getenv("V2X_SPATEM_DUPLICATE")',
            "[DENM_V3_TAMPER]",
            "[DENM_V3_DUPLICATE]",
            "[SPATEM_PHYSICAL_DUPLICATE]",
        ):
            self.assert_hook_compile_gated(token)


if __name__ == "__main__":
    unittest.main(verbosity=2)
