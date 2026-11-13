import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[4]
SOURCE = (
    ROOT / "scratch/vanetza_integration/wrappers/vanetza/access/"
    "ns3_access_adapter.cc"
)


class TestAccessTxCompletionContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = SOURCE.read_text()
        start = source.index("Ns3AccessAdapter::request(")
        end = source.index("\n}\n\n} // namespace ns3", start) + 2
        cls.function = source[start:end]

    def test_binding_is_consumed_and_completion_retained(self):
        self.assertIn("TxBridgeBinding binding = TxBridgeContext::Take()", self.function)
        self.assertIn("std::shared_ptr<TxCompletion> completion = binding.completion", self.function)
        self.assertIn("[ctx, completion, socket, ns3Packet", self.function)

    def test_sync_failure_paths_complete_before_return(self):
        for reason in ("null_packet", "socket_unavailable", "empty_frame"):
            with self.subTest(reason=reason):
                position = self.function.index(
                    f'completeFailure("{reason}")'
                )
                return_position = self.function.index(
                    "return;", position
                )
                self.assertLess(position, return_position)

    def test_contextless_send_completion_uses_terminal_reason(self):
        start = self.function.index("if (!ctx)")
        end = self.function.index("return;", start)
        branch = self.function[start:end]

        self.assertIn("completion->Complete(", branch)
        self.assertIn("TxCompletionStatus::Failure", branch)
        self.assertIn("TxCompletionStatus::Success", branch)
        self.assertIn('"SendTo failed"', branch)
        self.assertIn('"TX_COMPLETE"', branch)

    def test_primary_send_failure_completes_failure(self):
        send_failure = self.function.index("if (sent < 0)")
        opening = self.function.index("{", send_failure)

        depth = 0
        closing = None
        for position in range(opening, len(self.function)):
            if self.function[position] == "{":
                depth += 1
            elif self.function[position] == "}":
                depth -= 1
                if depth == 0:
                    closing = position + 1
                    break

        self.assertIsNotNone(closing)
        failure_block = self.function[send_failure:closing]

        self.assertIn("TxCompletionStatus::Failure", failure_block)
        self.assertIn("completion->Complete(", failure_block)
        self.assertIn("return;", failure_block)

    def test_primary_success_order_is_terminal(self):
        send_ok = self.function.index('"SEND_OK"')
        tx_complete = self.function.index('"TX_COMPLETE"', send_ok)
        completion = self.function.index(
            "completion->Complete(", tx_complete
        )

        self.assertLess(send_ok, tx_complete)
        self.assertLess(tx_complete, completion)
        self.assertIn("TxCompletionStatus::Success", self.function[completion:])

    def test_duplicate_test_hooks_do_not_complete_primary_binding(self):
        marker = "#if defined(V2X_ENABLE_SECURITY_TEST_HOOKS)"
        start = self.function.index(marker, self.function.index("Simulator::ScheduleNow"))
        end = self.function.index("#endif // V2X_ENABLE_SECURITY_TEST_HOOKS", start)
        hooks = self.function[start:end]

        self.assertIn("if (duplicateDenm)", hooks)
        self.assertIn("if (duplicateSpatem)", hooks)
        self.assertIn("if (duplicateCpm)", hooks)
        self.assertNotIn("completion", hooks)
        self.assertNotIn("Complete(", hooks)


if __name__ == "__main__":
    unittest.main(verbosity=2)
