import inspect
import socket as std_socket
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import v2x_bridge_16 as bridge


class TestV2xBridge16UdpContract(unittest.TestCase):
    def test_socket_binds_the_configured_ns3_return_endpoint(self):
        fake_socket = Mock()
        socket_api = SimpleNamespace(
            AF_INET=std_socket.AF_INET,
            SOCK_DGRAM=std_socket.SOCK_DGRAM,
            socket=Mock(return_value=fake_socket),
        )

        with patch.object(bridge, "socket", socket_api):
            result = bridge.create_bridge_socket()

        self.assertIs(result, fake_socket)
        socket_api.socket.assert_called_once_with(
            std_socket.AF_INET,
            std_socket.SOCK_DGRAM,
        )
        fake_socket.bind.assert_called_once_with(
            (bridge.SUMO_BRIDGE_IP, bridge.SUMO_BRIDGE_PORT)
        )

    def test_socket_is_closed_when_bind_fails(self):
        fake_socket = Mock()
        fake_socket.bind.side_effect = OSError("Address already in use")

        socket_api = SimpleNamespace(
            AF_INET=std_socket.AF_INET,
            SOCK_DGRAM=std_socket.SOCK_DGRAM,
            socket=Mock(return_value=fake_socket),
        )

        with patch.object(bridge, "socket", socket_api):
            with self.assertRaisesRegex(OSError, "Address already in use"):
                bridge.create_bridge_socket()

        fake_socket.close.assert_called_once()

    def test_bound_socket_receives_a_datagram_on_the_return_port(self):
        bridge_socket = bridge.create_bridge_socket()
        self.addCleanup(bridge_socket.close)

        self.assertEqual(
            bridge_socket.getsockname()[:2],
            (bridge.SUMO_BRIDGE_IP, bridge.SUMO_BRIDGE_PORT),
        )

        payload = b"UDP_RETURN_PATH_TEST"
        bridge_socket.settimeout(1.0)

        with std_socket.socket(
            std_socket.AF_INET,
            std_socket.SOCK_DGRAM,
        ) as sender:
            sender.sendto(
                payload,
                (bridge.SUMO_BRIDGE_IP, bridge.SUMO_BRIDGE_PORT),
            )

        received, _sender_address = bridge_socket.recvfrom(1024)
        self.assertEqual(received, payload)

    def test_main_drains_return_socket_after_final_step_boundary(self):
        main_source = inspect.getsource(bridge.main)

        final_send_index = main_source.rfind("sock.sendto(")
        self.assertNotEqual(final_send_index, -1)

        final_receive_index = main_source.find(
            "receive_from_ns3(sock)", final_send_index
        )
        self.assertNotEqual(
            final_receive_index,
            -1,
            "main must drain the return socket after the final step boundary",
        )

        finally_index = main_source.find("finally:", final_send_index)
        self.assertNotEqual(finally_index, -1)
        self.assertLess(final_receive_index, finally_index)


    def test_production_main_uses_the_bound_socket_factory(self):
        main_source = inspect.getsource(bridge.main)

        self.assertIn("sock = create_bridge_socket()", main_source)
        self.assertNotIn("sock = socket.socket(", main_source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
