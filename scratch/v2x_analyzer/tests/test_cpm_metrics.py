import json
import unittest

from v2x_analyzer.parser import LogEntry, ParseStats
from v2x_analyzer.metrics.cpm import compute
from v2x_analyzer.report import build_report
from v2x_analyzer.report import render_json, render_text


def event(
    module,
    name,
    *,
    pkt=1,
    node=1,
    station=9001,
    payload_type=None,
    payload_size=None,
    msg="",
    t=None,
):
    return LogEntry(
        step=1,
        t=t,
        level="INFO",
        module=module,
        event=name,
        veh="",
        node=node,
        station=station,
        seq=1,
        pkt=pkt,
        veh_pkt=None,
        pipe_step=None,
        pipe_stage=None,
        result=None,
        duration=None,
        payload_type=payload_type,
        payload_size=payload_size,
        msg=msg,
        line_no=1,
        source_file="fixture.log",
    )


class TestCpmMetrics(unittest.TestCase):
    def test_counts_stages_objects_sizes_and_intervals(self):
        entries = [
            # Ordem deliberadamente não sequencial.
            event(
                "TX", "TX_COMPLETE", pkt=1,
                payload_type="CPM", payload_size=314,
            ),
            event(
                "CPM", "CPM_ENCODE_OK", pkt=1,
                payload_type="CPM", payload_size=49,
                msg=(
                    "encoded_bytes=49 "
                    "included_objects=1 object_ids=1"
                ),
            ),
            event(
                "CPM", "CPM_GENERATE_OK", pkt=1,
                msg=(
                    "reference_time_its_ms=1000 "
                    "known_objects=4"
                ),
            ),
            event(
                "CPM", "CPM_RX_DISPATCH", pkt=1,
                node=2, payload_type="RAW",
                payload_size=314,
            ),
            event(
                "CPM", "CPM_ENCODE_OK", pkt=2,
                payload_type="CPM", payload_size=49,
                msg=(
                    "encoded_bytes=49 "
                    "included_objects=2 object_ids=1,2"
                ),
            ),
            event(
                "CPM", "CPM_GENERATE_OK", pkt=2,
                msg=(
                    "reference_time_its_ms=1100 "
                    "known_objects=2"
                ),
            ),
            event(
                "TX", "TX_COMPLETE", pkt=2,
                payload_type="CPM", payload_size=314,
            ),
            event(
                "CPM", "CPM_RX_DISPATCH", pkt=2,
                node=2, payload_type="RAW",
                payload_size=314,
            ),
            event(
                "CPM", "CPM_DELIVER_OK", pkt=1,
                msg="included_objects=1 object_ids=1",
            ),
            event(
                "CPM", "CPM_DELIVER_OK", pkt=2,
                msg="included_objects=2 object_ids=1,2",
            ),
        ]

        metrics = compute(entries)

        self.assertEqual(metrics.generate_ok, 2)
        self.assertEqual(metrics.encode_ok, 2)
        self.assertEqual(metrics.tx_complete, 2)
        self.assertEqual(metrics.physical_rx_packets, 2)
        self.assertEqual(metrics.deliver_ok, 2)

        self.assertEqual(metrics.known_objects_total, 6)
        self.assertEqual(metrics.known_objects_samples, 2)
        self.assertEqual(metrics.known_objects_mean, 3.0)

        self.assertEqual(metrics.included_objects_total, 3)
        self.assertEqual(metrics.included_objects_samples, 2)

        self.assertEqual(metrics.delivered_objects_total, 3)
        self.assertEqual(metrics.delivered_objects_samples, 2)

        self.assertEqual(
            metrics.encoded_size_distribution,
            {49: 2},
        )
        self.assertEqual(metrics.encoded_size_samples, 2)
        self.assertEqual(metrics.encoded_size_max, 49)

        self.assertEqual(metrics.generation_interval_samples, 1)
        self.assertEqual(metrics.generation_interval_min_ms, 100)
        self.assertEqual(metrics.generation_interval_mean_ms, 100)
        self.assertEqual(metrics.generation_interval_max_ms, 100)

    def test_failure_causes_remain_distinct(self):
        entries = [
            event("CPM", "CPM_BTP_RX_FAIL"),
            event("CPM", "CPM_PROFILE_CHECK_FAIL"),
            event("CPM", "CPM_CRYPTO_VERIFY_FAIL"),
            event("CPM", "CPM_DECODE_FAIL"),
            event("CPM", "CPM_SEMANTIC_VALIDATION_FAIL"),
            event("CPM", "CPM_SENDER_IDENTITY_FAIL"),
            event("CPM", "CPM_SELF_RECEPTION_DROP"),
            event("CPM", "CPM_APPLICATION_QUEUE_FULL"),
            event("CPM", "CPM_REPLAY_DROP"),
        ]

        metrics = compute(entries)

        self.assertEqual(metrics.btp_rx_fail, 1)
        self.assertEqual(metrics.profile_check_fail, 1)
        self.assertEqual(metrics.security_fail, 1)
        self.assertEqual(metrics.decode_fail, 1)
        self.assertEqual(metrics.semantic_validation_fail, 1)
        self.assertEqual(metrics.sender_identity_fail, 1)
        self.assertEqual(metrics.self_reception_drop, 1)
        self.assertEqual(metrics.application_queue_full, 1)
        self.assertEqual(metrics.replay_drop, 1)

    def test_missing_or_malformed_fields_do_not_crash(self):
        entries = [
            event(
                "CPM", "CPM_GENERATE_OK",
                msg=(
                    "known_objects=unknown "
                    "reference_time_its_ms=invalid"
                ),
            ),
            event(
                "CPM", "CPM_ENCODE_OK",
                payload_type="CPM",
                msg=(
                    "encoded_bytes=invalid "
                    "included_objects=unknown"
                ),
            ),
        ]

        metrics = compute(entries)

        self.assertEqual(metrics.generate_ok, 1)
        self.assertEqual(metrics.encode_ok, 1)
        self.assertEqual(metrics.known_objects_samples, 0)
        self.assertEqual(metrics.included_objects_samples, 0)
        self.assertEqual(metrics.encoded_size_samples, 0)
        self.assertEqual(metrics.physical_rx_packets, 0)
        self.assertEqual(metrics.generation_interval_samples, 0)


    def test_repeated_timestamps_do_not_create_zero_intervals(self):
        entries = [
            event(
                "CPM", "CPM_GENERATE_OK", pkt=1,
                msg="reference_time_its_ms=1000 known_objects=1",
            ),
            event(
                "CPM", "CPM_GENERATE_OK", pkt=2,
                msg="reference_time_its_ms=1000 known_objects=2",
            ),
            event(
                "CPM", "CPM_GENERATE_OK", pkt=3,
                msg="reference_time_its_ms=1100 known_objects=3",
            ),
        ]

        metrics = compute(entries)

        self.assertEqual(metrics.generate_ok, 3)
        self.assertEqual(metrics.generation_interval_samples, 1)
        self.assertEqual(metrics.generation_interval_min_ms, 100)
        self.assertEqual(metrics.generation_interval_max_ms, 100)

    def test_mixed_protocols_and_report_serialization(self):
        entries = [
            event("CAM", "GENERATE_OK", pkt=10),
            event("DENM", "GENERATE_OK", pkt=11),
            event("SPATEM", "SPATEM_GENERATE_OK", pkt=12),
            event(
                "CPM", "CPM_GENERATE_OK", pkt=13,
                msg=(
                    "reference_time_its_ms=1000 "
                    "known_objects=3"
                ),
            ),
            event(
                "CPM", "CPM_ENCODE_OK", pkt=13,
                payload_type="CPM", payload_size=2,
                msg="encoded_bytes=2 included_objects=2",
            ),
            event(
                "TX", "TX_COMPLETE", pkt=13,
                payload_type="CPM", payload_size=314,
            ),
            event(
                "CPM", "CPM_RX_DISPATCH", pkt=13,
                node=2, payload_type="RAW", payload_size=314,
            ),
            event(
                "CPM", "CPM_DELIVER_OK", pkt=13, node=2,
                msg="included_objects=2 object_ids=1,2",
            ),
        ]

        stats = ParseStats(
            files=["mixed.log"],
            total_lines=len(entries),
            parsed_lines=len(entries),
            unparsed_lines=0,
            unparsed_samples=[],
        )
        report = build_report(entries, stats)

        # Shared TX metrics count all protocol generations.
        self.assertEqual(report.tx.generate_ok, 4)

        # CPM is reported independently.
        self.assertEqual(report.cpm.generate_ok, 1)
        self.assertEqual(report.cpm.encode_ok, 1)
        self.assertEqual(report.cpm.tx_complete, 1)
        self.assertEqual(report.cpm.physical_rx_packets, 1)
        self.assertEqual(report.cpm.deliver_ok, 1)
        self.assertEqual(report.cpm.included_objects_total, 2)
        self.assertEqual(report.cpm.delivered_objects_total, 2)

        text_report = render_text(report)
        self.assertIn("Physical RX packets", text_report)
        self.assertIn("Semantic validation fail", text_report)

        json_report = json.loads(render_json(report))
        self.assertEqual(json_report["cpm"]["generate_ok"], 1)
        self.assertEqual(
            json_report["cpm"]["physical_rx_packets"], 1
        )
        self.assertEqual(
            json_report["cpm"]["delivered_objects_total"], 2
        )


    def test_successful_rx_stages_are_counted_separately(self):
        entries = [
            event("CPM", "CPM_GEONET_RX_OK"),
            event("CPM", "CPM_BTP_RX_OK"),
            event("CPM", "CPM_PROFILE_CHECK_OK"),
            event("CPM", "CPM_DECODE_OK"),
            event("CPM", "CPM_REPLAY_OK"),
            event("CPM", "CPM_RECEPTION_OK"),
            event("CPM", "CPM_DELIVER_OK"),
        ]

        metrics = compute(entries)

        self.assertEqual(metrics.geonet_rx_ok, 1)
        self.assertEqual(metrics.btp_rx_ok, 1)
        self.assertEqual(metrics.profile_check_ok, 1)
        self.assertEqual(metrics.decode_ok, 1)
        self.assertEqual(metrics.replay_ok, 1)
        self.assertEqual(metrics.reception_ok, 1)
        self.assertEqual(metrics.deliver_ok, 1)

    def test_duplicate_object_ids_do_not_override_reported_count(self):
        entries = [
            event(
                "CPM", "CPM_ENCODE_OK",
                payload_type="CPM",
                payload_size=20,
                msg="encoded_bytes=20 included_objects=1 "
                    "object_ids=7,7",
            ),
            event(
                "CPM", "CPM_DELIVER_OK",
                msg="included_objects=1 object_ids=7,7",
            ),
            event(
                "CPM", "CPM_RX_DISPATCH",
                node=2, payload_type="RAW",
                payload_size=314,
            ),
        ]

        metrics = compute(entries)

        self.assertEqual(metrics.included_objects_total, 1)
        self.assertEqual(metrics.delivered_objects_total, 1)
        self.assertEqual(metrics.physical_rx_packets, 1)

    def test_regressing_timestamps_are_not_sorted_into_intervals(self):
        entries = [
            event(
                "CPM", "CPM_GENERATE_OK", pkt=1,
                msg="reference_time_its_ms=1100 known_objects=1",
            ),
            event(
                "CPM", "CPM_GENERATE_OK", pkt=2,
                msg="reference_time_its_ms=1000 known_objects=1",
            ),
        ]

        metrics = compute(entries)

        self.assertEqual(metrics.generate_ok, 2)
        self.assertEqual(metrics.generation_interval_samples, 0)
        self.assertIsNone(metrics.generation_interval_mean_ms)


    def test_incomplete_pipeline_counts_observed_events(self):
        entries = [
            event("CPM", "CPM_DECODE_FAIL", pkt=9),
            event(
                "CPM", "CPM_DELIVER_OK", pkt=10,
                msg="included_objects=2 object_ids=3,4",
            ),
        ]
        metrics = compute(entries)

        self.assertEqual(metrics.decode_fail, 1)
        self.assertEqual(metrics.deliver_ok, 1)
        self.assertEqual(metrics.delivered_objects_total, 2)
        self.assertEqual(metrics.physical_rx_packets, 0)
        self.assertEqual(metrics.decode_ok, 0)
        self.assertEqual(metrics.replay_ok, 0)
        self.assertEqual(metrics.reception_ok, 0)


    def test_cpm_pipeline_satisfies_shared_exit_criteria(self):
        entries = [
            event("CPM", "CPM_GENERATE_OK", pkt=1),
            event(
                "CPM", "CPM_ENCODE_OK", pkt=1,
                payload_type="CPM", payload_size=4,
                msg="encoded_bytes=4 included_objects=1",
            ),
            event("BTP", "BTP_TX_BEGIN", pkt=1),
            event("BTP", "BTP_TX_OK", pkt=1),
            event("GEONET", "GEONET_TX_BEGIN", pkt=1),
            event("GEONET", "GEONET_TX_OK", pkt=1),
            event("ACCESS", "ACCESS_TX_BEGIN", pkt=1),
            event("ACCESS", "ACCESS_TX_OK", pkt=1),
            event("ACCESS", "SEND_BEGIN", pkt=1),
            event("ACCESS", "SEND_OK", pkt=1),
            event(
                "TX", "TX_COMPLETE", pkt=1,
                payload_type="CPM", payload_size=314,
            ),
            event("RX", "RAW_RX", pkt=1, node=2),
            event("ACCESS", "ACCESS_RX_BEGIN", pkt=1, node=2),
            event("ACCESS", "ACCESS_RX_OK", pkt=1, node=2),
            event("CPM", "CPM_GEONET_RX_OK", pkt=1, node=2),
            event("CPM", "CPM_BTP_RX_OK", pkt=1, node=2),
            event("CPM", "CPM_PROFILE_CHECK_OK", pkt=1, node=2),
            event("CPM", "CPM_DECODE_OK", pkt=1, node=2),
            event("CPM", "CPM_REPLAY_OK", pkt=1, node=2),
            event("CPM", "CPM_RECEPTION_OK", pkt=1, node=2),
            event("CPM", "CPM_DELIVER_OK", pkt=1, node=2),
        ]
        stats = ParseStats(
            files=["cpm-pipeline.log"],
            total_lines=len(entries),
            parsed_lines=len(entries),
            unparsed_lines=0,
            unparsed_samples=[],
        )
        report = build_report(entries, stats)
        criteria = {
            item.name: item for item in report.exit_.criteria
        }

        self.assertEqual(report.tx.generate_ok, 1)
        self.assertEqual(report.tx.encode_ok, 1)
        self.assertEqual(report.rx.decode_ok, 1)
        self.assertEqual(report.rx.replay_ok, 1)
        self.assertEqual(report.rx.deliver_ok, 1)
        self.assertTrue(
            criteria["Cadeia TX consistente"].passed
        )
        self.assertTrue(
            criteria["Cadeia RX consistente"].passed
        )
        self.assertTrue(
            criteria["Sem falhas TX/RX observadas"].passed
        )

    def test_cpm_semantic_failure_fails_p3(self):
        entries = [
            event(
                "CPM", "CPM_SEMANTIC_VALIDATION_FAIL",
                pkt=1, node=2,
            ),
        ]
        stats = ParseStats(
            files=["cpm-failure.log"],
            total_lines=1,
            parsed_lines=1,
            unparsed_lines=0,
            unparsed_samples=[],
        )
        report = build_report(entries, stats)
        criteria = {
            item.name: item for item in report.exit_.criteria
        }

        self.assertEqual(
            report.rx.semantic_validation_fail, 1
        )
        self.assertFalse(
            criteria["Sem falhas TX/RX observadas"].passed
        )


if __name__ == "__main__":
    unittest.main()
