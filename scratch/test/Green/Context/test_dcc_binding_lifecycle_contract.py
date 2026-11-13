import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[4]
SOURCE = ROOT / "scratch/vanetza_integration/wrappers/vanetza/access/ns3_dcc_adapter.cc"


class TestDccBindingLifecycleContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text()
        start = cls.source.index("Ns3DccAdapter::request(")
        end = cls.source.index(
            "Ns3DccAdapter::UpdateChannelLoad()", start
        )
        cls.request = cls.source[start:end]

    def test_request_delegates_queue_decision_to_flow_control(self):
        self.assertRegex(
            self.request,
            r"m_flowControl\.request\s*\(\s*"
            r"dccRequest\s*,\s*std::move\(packet\)\s*\)\s*;",
        )
        self.assertIn("ScheduleNextRuntimeEvent();", self.request)

    def test_request_does_not_infer_packet_state_from_global_runtime(self):
        self.assertNotIn("m_runtime.next()", self.request)

    def test_transmit_hook_takes_packet_binding(self):
        self.assertRegex(
            self.source,
            r"Ns3DccAdapter::OnPacketTransmit[\s\S]*?"
            r"m_packetBindings\.Take\(packet\)",
        )

    def test_drop_hook_takes_and_resolves_packet_binding(self):
        self.assertRegex(
            self.source,
            r"Ns3DccAdapter::OnPacketDrop[\s\S]*?"
            r"m_packetBindings\.Take\(packet\)",
        )
        self.assertIn("dcc_lifetime_expired", self.source)

    def test_shutdown_drains_pending_bindings(self):
        self.assertIn("m_packetBindings.Drain()", self.source)
        self.assertIn("dcc_adapter_shutdown", self.source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
