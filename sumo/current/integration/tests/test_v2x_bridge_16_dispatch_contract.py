import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import v2x_bridge_16 as bridge


class TestNs3VehicleStateParser(unittest.TestCase):
    def test_parses_the_six_field_ns3_serializer_contract(self):
        state = bridge.parse_ns3_vehicle_state(
            "veh0,12.5,-3.25,7.0,90.0,-0.5"
        )

        self.assertEqual(state.vehicle_id, "veh0")
        self.assertAlmostEqual(state.x, 12.5)
        self.assertAlmostEqual(state.y, -3.25)
        self.assertAlmostEqual(state.speed, 7.0)
        self.assertAlmostEqual(state.heading, 90.0)
        self.assertAlmostEqual(state.accel, -0.5)

    def test_rejects_wrong_field_counts(self):
        invalid_messages = (
            "veh0,1,2,3,4",
            "veh0,1,2,3,4,5,600000",
        )

        for message in invalid_messages:
            with self.subTest(message=message):
                with self.assertRaises(ValueError):
                    bridge.parse_ns3_vehicle_state(message)

    def test_rejects_empty_vehicle_id(self):
        with self.assertRaises(ValueError):
            bridge.parse_ns3_vehicle_state(",1,2,3,4,5")

        with self.assertRaises(ValueError):
            bridge.parse_ns3_vehicle_state("   ,1,2,3,4,5")

    def test_rejects_invalid_or_non_finite_numeric_fields(self):
        invalid_messages = (
            "veh0,not-a-number,2,3,4,5",
            "veh0,1,,3,4,5",
            "veh0,nan,2,3,4,5",
            "veh0,1,2,inf,4,5",
            "veh0,1,2,3,4,-inf",
        )

        for message in invalid_messages:
            with self.subTest(message=message):
                with self.assertRaises(ValueError):
                    bridge.parse_ns3_vehicle_state(message)


class TestNs3VehicleStateDispatch(unittest.TestCase):
    def make_socket(self, messages):
        sock = Mock()
        datagrams = [
            (message.encode("utf-8"), ("127.0.0.1", 7000))
            for message in messages
        ]
        sock.recvfrom.side_effect = datagrams + [BlockingIOError()]
        return sock

    def test_receive_dispatches_a_valid_vehicle_state(self):
        message = "veh0,12.5,-3.25,7.0,90.0,-0.5"
        sock = self.make_socket([message])

        with patch.object(bridge, "handle_ns3_vehicle_state") as handler:
            with patch.object(bridge, "log") as logger:
                bridge.receive_from_ns3(sock)

        handler.assert_called_once()
        state = handler.call_args.args[0]
        self.assertEqual(state.vehicle_id, "veh0")
        self.assertAlmostEqual(state.x, 12.5)
        logger.assert_any_call(f"NS3 -> SUMO | {message}")

    def test_invalid_datagram_does_not_block_later_valid_datagrams(self):
        invalid = "veh0,1,2,3,4,nan"
        valid = "veh1,10,20,5,180,-1"
        sock = self.make_socket([invalid, valid])

        with patch.object(bridge, "handle_ns3_vehicle_state") as handler:
            with patch.object(bridge, "log"):
                bridge.receive_from_ns3(sock)

        handler.assert_called_once()
        state = handler.call_args.args[0]
        self.assertEqual(state.vehicle_id, "veh1")
        self.assertEqual(sock.recvfrom.call_count, 3)


if __name__ == "__main__":
    unittest.main()
