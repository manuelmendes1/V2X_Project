#!/usr/bin/env python3

import unittest
from pathlib import Path


def find_repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


ROOT = find_repository_root(Path(__file__))
SOURCE = ROOT / (
    "scratch/vanetza_integration/context/"
    "simulation_controller/simulation_controller.cc"
)
HEADER = ROOT / (
    "scratch/vanetza_integration/context/"
    "simulation_controller/simulation_controller.h"
)


def function_body(source, signature):
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Function not found: {signature}")

    opening = source.find("{", start)
    if opening < 0:
        raise AssertionError(f"Opening brace not found: {signature}")

    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]

    raise AssertionError(f"Unclosed function body: {signature}")


class TestSpatemPollBatch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text()
        cls.header = HEADER.read_text()

    def test_poll_sends_snapshot_after_draining_udp_batch(self):
        body = function_body(
            self.source,
            "SimulationController::PollLoop()",
        )
        poll_position = body.find("SumoAdapter::Poll();")
        send_position = body.find("SendSpatemSnapshot();")

        self.assertGreaterEqual(poll_position, 0)
        self.assertGreaterEqual(
            send_position,
            0,
            "PollLoop must request one SPATEM snapshot per UDP batch",
        )
        self.assertLess(poll_position, send_position)
        self.assertEqual(body.count("SendSpatemSnapshot();"), 1)

    def test_tls_callback_updates_builder_without_sending_each_record(self):
        body = function_body(
            self.source,
            "SimulationController::HandleTrafficLightUpdateFromSumo(",
        )

        self.assertIn("m_spatemIntersectionBuilder->Update(", body)
        self.assertIn(
            "m_spatemDirty = true;",
            body,
            "Each valid TLS update must mark the accumulated snapshot dirty",
        )
        self.assertNotIn(
            "m_spatemRuntime->Send(",
            body,
            "The per-link callback must not transmit immediately",
        )

    def test_snapshot_sender_transmits_accumulated_intersection(self):
        self.assertIn(
            "SendSpatemSnapshot()",
            self.header,
            "SimulationController must declare the batch sender",
        )
        body = function_body(
            self.source,
            "SimulationController::SendSpatemSnapshot()",
        )

        self.assertIn("m_spatemDirty", body)
        self.assertIn("m_spatemIntersectionBuilder->GetIntersection()", body)
        self.assertIn("m_spatemRuntime->Send(context, input)", body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
