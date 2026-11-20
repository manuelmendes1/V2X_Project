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
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestVamRxDispatchWiring(unittest.TestCase):
    def setUp(self):
        self.root = repository_root(Path(__file__))
        self.runtime = (
            self.root
            / "scratch/vanetza_integration/vanetza_runtime/vanetza_runtime.cc"
        ).read_text(encoding="utf-8")
        self.policy = (
            self.root
            / "scratch/vanetza_integration/vam/transport/vam_transport_policy.h"
        ).read_text(encoding="utf-8")

    def test_runtime_registers_vam_port_2018(self):
        self.assertIn(
            '#include "vanetza_integration/vam/transport/vam_transport_policy.h"',
            self.runtime,
            "VAM_TRANSPORT_POLICY_NOT_INCLUDED_BY_RUNTIME",
        )
        self.assertRegex(
            self.runtime,
            r"SetBtpRxHandler\s*\(\s*kVamBtpPort\s*,",
            "VAM_BTP_2018_HANDLER_NOT_REGISTERED",
        )
        self.assertRegex(
            self.policy,
            r"kVamBtpPort\s*=\s*2018",
            "VAM_BTP_PORT_CONSTANT_MISSING",
        )

    def test_vam_handler_preserves_packet_and_geonet_metadata(self):
        match = re.search(
            r"SetBtpRxHandler\s*\(\s*kVamBtpPort\s*,\s*"
            r"\[this\]\s*\(\s*const\s+"
            r"VanetzaGeoNetAdapter::BtpRxData\s*&\s*rx\s*\)"
            r"\s*\{(?P<body>.*?)\}\s*\);",
            self.runtime,
            re.DOTALL,
        )
        self.assertIsNotNone(
            match,
            "VAM_RX_HANDLER_BODY_NOT_FOUND",
        )
        body = match.group("body")

        required = {
            "RX_BRIDGE_CONTEXT": r"RxBridgeContext::Current\s*\(\s*\)",
            "BTP_DESTINATION_PORT": r"ctx->btpDestinationPort\s*=\s*rx\.destinationPort",
            "GEONET_UPPER_PROTOCOL": r"ctx->geonet\.upperProtocol\s*=\s*rx\.upperProtocol",
            "GEONET_TRANSPORT_TYPE": r"ctx->geonet\.transportType\s*=\s*rx\.transportType",
            "TRAFFIC_CLASS": r"ctx->geonet\.trafficClass\s*=\s*rx\.trafficClass",
            "ITS_AID": r"ctx->geonet\.itsAid\s*=\s*rx\.itsAid",
            "PACKET_LIFETIME": r"ctx->geonet\.lifetimeMs\s*=\s*rx\.lifetimeMs",
            "SENDER_MID": r"ctx->geonet\.senderMid\s*=\s*rx\.sourceMid",
            "VAM_DISPATCH_EVENT": r'"VAM_RX_DISPATCH"',
        }

        for label, pattern in required.items():
            with self.subTest(field=label):
                self.assertRegex(body, pattern, f"VAM_RX_{label}_NOT_PRESERVED")

        normalized_body = re.sub(r"\s+", "", body)
        self.assertIn(
            "VamRxBoundary::Process(*ctx,rx.payload)",
            normalized_body,
            "VAM_PAYLOAD_NOT_FORWARDED_TO_VAM_RX_BOUNDARY",
        )
        self.assertNotIn(
            "RxPipeline::ProcessDirect",
            body,
            "VAM_MUST_NOT_ENTER_CAM_RX_PIPELINE",
        )

    def test_existing_cam_denm_cpm_and_spatem_handlers_remain(self):
        for port in ("2001", "2002", "2004"):
            with self.subTest(port=port):
                self.assertRegex(
                    self.runtime,
                    rf"SetBtpRxHandler\s*\(\s*{port}\s*,",
                    f"EXISTING_BTP_HANDLER_{port}_MISSING",
                )

        self.assertRegex(
            self.runtime,
            r"SetBtpRxHandler\s*\(\s*kCpmBtpPort\s*,",
            "EXISTING_CPM_BTP_HANDLER_MISSING",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
