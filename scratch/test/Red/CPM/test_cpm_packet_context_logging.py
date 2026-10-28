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


class TestCpmPacketContextLogging(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.base = cls.root / "scratch/vanetza_integration"

        def read(relative):
            return (cls.root / relative).read_text(encoding="utf-8")

        cls.logger = read("scratch/vanetza_integration/core/v2x_log.cc")
        cls.cpm_service = read(
            "scratch/vanetza_integration/cpm/service/cpm_service.cc"
        )
        cls.cpm_rx = read(
            "scratch/vanetza_integration/cpm/rx/cpm_rx_pipeline.cc"
        )
        cls.access = read(
            "scratch/vanetza_integration/wrappers/vanetza/access/"
            "ns3_access_adapter.cc"
        )
        cls.runtime = read(
            "scratch/vanetza_integration/vanetza_runtime/vanetza_runtime.cc"
        )
        cls.btp_adapter = read(
            "scratch/vanetza_integration/wrappers/vanetza/btp/"
            "vanetza_btp_adapter.cc"
        )
        cls.geonet_adapter = read(
            "scratch/vanetza_integration/wrappers/vanetza/geonet/"
            "vanetza_geonet_adapter.cc"
        )

    def assert_tokens(self, source, tokens):
        for token in tokens:
            with self.subTest(token=token):
                self.assertIn(token, source)

    def test_structured_logger_exposes_packet_identity_and_metadata(self):
        self.assert_tokens(self.logger, [
            "[STATION=",
            "[SENDER_STATION_ID=",
            "[RECEIVER_STATION_ID=",
            "[PAYLOAD_TYPE=",
            "[PAYLOAD_SIZE=",
            "[BTP_PORT=",
            "[ITS_AID=",
            "[GN_UPPER_PROTOCOL=",
            "[GN_TRANSPORT=",
            "[GN_ADDR_MID=",
            "[SENDER_LINK_MAC=",
            "[VERIFY_PERFORMED=",
            "[VERIFY_OK=",
            "[VERIFY_REPORT=",
            "[VERIFY_AID=",
        ])

    def test_rx_geonet_metadata_survives_btp_dispatch(self):
        self.assert_tokens(self.btp_adapter, [
            "class VanetzaBtpAdapter::GeoNetTransportBridge",
            "indication.upper_protocol",
            "indication.transport_type",
            "m_dispatcher.indicate(indication, std::move(packet))",
            "data.upperProtocol =",
            "data.transportType =",
        ])
        self.assert_tokens(self.geonet_adapter, [
            "data.upperProtocol =",
            "data.transportType =",
        ])
        self.assert_tokens(self.runtime, [
            "ctx->geonet.upperProtocol = rx.upperProtocol;",
            "ctx->geonet.transportType = rx.transportType;",
        ])

    def test_cpm_tx_logs_generation_and_encoding_results(self):
        self.assert_tokens(self.cpm_service, [
            "V2XWriteLog",
            "CPM_GENERATE_OK",
            "CPM_GENERATE_FAIL",
            "CPM_ENCODE_OK",
            "CPM_ENCODE_FAIL",
            "reference_time_its_ms=",
            "known_objects=",
            "included_objects=",
            "object_ids=",
            "encoded_bits=",
        ])

    def test_cpm_tx_logs_per_object_temporal_fields(self):
        self.assert_tokens(self.cpm_service, [
            "ObjectTemporalSummary(input.perceivedObjects)",
            "object_temporal_ms=",
        ])

    def test_cpm_rx_logs_per_object_temporal_fields(self):
        self.assert_tokens(self.cpm_rx, [
            "ObjectTemporalSummary(message.perceivedObjects)",
            "object_temporal_ms=",
        ])

    def test_cpm_rx_logs_object_semantics_with_packet_context(self):
        self.assert_tokens(self.cpm_rx, [
            "V2XWriteLog",
            "CPM_RECEPTION_OK",
            "CPM_DELIVER_OK",
            "known_objects=",
            "included_objects=",
            "reference_time_its_ms=",
            "object_ids=",
        ])

    def test_packet_id_tag_tx_rx_correlation_remains_intact(self):
        self.assert_tokens(self.access, [
            "PacketIdTag tag(ctx->packetId)",
            "AddPacketTag(tag)",
            "[TX_PACKET_ID_TAG]",
        ])
        self.assert_tokens(self.runtime, [
            "PeekPacketTag(rxTag)",
            "ctx.packetId = taggedPacketId",
            "[RX_PACKET_ID_TAG]",
            "[RX_PACKET_ID_TAG_MISSING]",
        ])

    def test_cpm_rx_requires_successful_verification_for_aid_639(self):
        self.assert_tokens(self.cpm_rx, [
            "if (!context.verificationPerformed)",
            "if (!context.verificationSucceeded)",
            "if (context.verifiedItsAid != kCpmItsAid)",
            "cpm_cryptographic_verification_missing",
            "cpm_cryptographic_verification_failed",
            "cpm_verified_its_aid_mismatch",
        ])


if __name__ == "__main__":
    unittest.main(verbosity=2)
