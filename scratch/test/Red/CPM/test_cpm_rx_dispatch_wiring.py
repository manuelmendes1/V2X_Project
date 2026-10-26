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


class TestCpmRxDispatchWiring(unittest.TestCase):
    def test_rx_pipeline_is_declared_and_included_in_build(self):
        root = repository_root(Path(__file__))
        base = root / "scratch/vanetza_integration/cpm/rx"
        cmake = (root / "scratch/CMakeLists.txt").read_text()

        self.assertTrue(
            (base / "cpm_rx_pipeline.h").is_file(),
            "CPM_RX_PIPELINE_HEADER_MISSING",
        )
        self.assertTrue(
            (base / "cpm_rx_pipeline.cc").is_file(),
            "CPM_RX_PIPELINE_SOURCE_MISSING",
        )
        self.assertIn(
            "cpm/rx/cpm_rx_pipeline.cc",
            cmake.replace("\\", "/"),
            "CPM_RX_PIPELINE_NOT_INCLUDED_IN_BUILD",
        )

    def test_runtime_dispatches_port_2009_to_cpm_pipeline(self):
        root = repository_root(Path(__file__))
        runtime = (
            root / "scratch/vanetza_integration/"
            "vanetza_runtime/vanetza_runtime.cc"
        ).read_text()

        self.assertIn(
            '#include "vanetza_integration/cpm/rx/cpm_rx_pipeline.h"',
            runtime,
        )
        self.assertRegex(
            runtime,
            r"SetBtpRxHandler\s*\(\s*(?:kCpmBtpPort|2009)\s*,",
            "CPM_BTP_2009_RX_HANDLER_MISSING",
        )
        self.assertRegex(
            runtime,
            r"CpmRxPipeline::Process\s*\(",
            "CPM_RX_PIPELINE_NOT_INVOKED_BY_RUNTIME",
        )
        self.assertRegex(
            runtime,
            r"CpmRxPipeline::Process\s*\(\s*\*ctx\s*,\s*"
            r"rx\.payload\s*,\s*message\s*,",
            "CPM_RX_PACKET_CONTEXT_OR_PAYLOAD_NOT_FORWARDED",
        )
        self.assertIn(
            "m_vehicle.security.cpmReplay",
            runtime,
            "CPM_RX_REPLAY_CONTEXT_NOT_FORWARDED",
        )
        self.assertIn(
            "m_vehicle.cpmReception",
            runtime,
            "CPM_RX_APPLICATION_QUEUE_NOT_FORWARDED",
        )
        self.assertIn(
            "VanetzaRuntime::TryReceiveCpm",
            runtime,
            "CPM_RUNTIME_CONSUMER_API_NOT_IMPLEMENTED",
        )
        runtime_header = (
            root / "scratch/vanetza_integration/"
            "vanetza_runtime/vanetza_runtime.h"
        ).read_text()
        self.assertIn(
            "TryReceiveCpm(CpmMessage& message)",
            runtime_header,
            "CPM_RUNTIME_CONSUMER_API_NOT_DECLARED",
        )

    def test_existing_cam_denm_spatem_dispatches_remain_present(self):
        root = repository_root(Path(__file__))
        runtime = (
            root / "scratch/vanetza_integration/"
            "vanetza_runtime/vanetza_runtime.cc"
        ).read_text()

        for port in ("2001", "2002", "2004"):
            with self.subTest(port=port):
                self.assertRegex(
                    runtime,
                    rf"SetBtpRxHandler\s*\(\s*{port}\s*,",
                    f"EXISTING_BTP_HANDLER_{port}_MISSING",
                )


if __name__ == "__main__":
    unittest.main(verbosity=2)
