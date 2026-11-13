import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[4]
SENDER = (
    ROOT
    / "scratch/vanetza_integration/cam/sender/cam_sender.cc"
)


class TestCamSenderTxCompletionAudit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SENDER.read_text()

    def test_audit_is_opt_in(self):
        self.assertIn("V2X_TX_COMPLETION_AUDIT", self.source)
        self.assertIn("std::getenv", self.source)

    def test_sender_creates_tx_completion(self):
        self.assertRegex(
            self.source,
            r"make_shared\s*<\s*TxCompletion\s*>",
        )

    def test_completion_is_bound_to_packet_context(self):
        self.assertIn(
            "TxBridgeContext::Push(ctx, completion)",
            self.source,
        )

    def test_callback_logs_correlated_terminal_result(self):
        self.assertIn('"TX_COMPLETION_CALLBACK"', self.source)
        self.assertIn("completedAtSimUs", self.source)
        self.assertIn("V2XWriteLog", self.source)

    def test_btp_rejection_completes_failure(self):
        self.assertIn("TxCompletionStatus::Failure", self.source)
        self.assertRegex(
            self.source,
            r"completion\s*->\s*Complete\s*\(",
        )

    def test_router_acceptance_without_dcc_handoff_resolves_pending_binding(self):
        send_call = self.source.index(
            "bool sent = vehicle.runtime->SendBtpB("
        )
        failure_branch = self.source.index(
            "if (!sent)", send_call
        )
        handoff_guard = self.source.find(
            "TxBridgeContext::TakeIfMatches(ctx, completion)",
            failure_branch,
        )

        self.assertNotEqual(
            handoff_guard,
            -1,
            "Missing identity-checked cleanup when Router accepts "
            "but does not hand the packet to DCC",
        )

        self.assertGreater(handoff_guard, failure_branch)

        failure_reason = self.source.find(
            "router_accepted_without_dcc_handoff",
            handoff_guard,
        )
        self.assertNotEqual(failure_reason, -1)

        completion_failure = self.source.find(
            "TxCompletionStatus::Failure",
            handoff_guard,
        )
        self.assertNotEqual(completion_failure, -1)

        self.assertIn("orphanedBinding.context", self.source)

    def test_legacy_path_remains_available(self):
        self.assertIn("TxBridgeContext::Push(ctx);", self.source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
