#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import v2x_bridge_16 as bridge


class TestV2XBridge16TrafficLights(unittest.TestCase):

    def test_serializes_tls_record_using_cpp_contract(self):
        message = bridge.serialize_tls_message(
            tls_id="0",
            link_index=3,
            raw_state="G",
            phase_index=0,
            phase_duration=31.0,
            next_switch=31.0,
            program_id=0,
            simulation_time=0.0,
        )

        self.assertEqual(
            message,
            "SUMO_TLS,0,3,G,0,31.000000,31.000000,0,0.000000",
        )

    def test_resolves_explicit_tls_id(self):
        result = bridge.resolve_traffic_light_id(
            ["0", "east"],
            "east",
        )

        self.assertEqual(result, "east")

    def test_resolves_single_tls_when_no_id_is_configured(self):
        result = bridge.resolve_traffic_light_id(["0"], None)

        self.assertEqual(result, "0")

    def test_rejects_unknown_explicit_tls_id(self):
        with self.assertRaises(ValueError):
            bridge.resolve_traffic_light_id(["0"], "missing")

    def test_requires_explicit_id_when_multiple_tls_exist(self):
        with self.assertRaises(ValueError):
            bridge.resolve_traffic_light_id(["0", "east"], None)

    def test_no_tls_returns_none(self):
        self.assertIsNone(
            bridge.resolve_traffic_light_id([], None)
        )

    @patch.object(bridge.traci.simulation, "getTime", return_value=1.5)
    @patch.object(
        bridge.traci.trafficlight,
        "getRedYellowGreenState",
        return_value="G",
    )
    @patch.object(
        bridge.traci.trafficlight,
        "getNextSwitch",
        return_value=6.0,
    )
    @patch.object(
        bridge.traci.trafficlight,
        "getPhaseDuration",
        return_value=8.0,
    )
    @patch.object(
        bridge.traci.trafficlight,
        "getPhase",
        return_value=2,
    )
    @patch.object(
        bridge.traci.trafficlight,
        "getProgram",
        return_value="0",
    )
    def test_publishes_one_record_per_controlled_link(
        self,
        _program,
        _phase,
        _duration,
        _next_switch,
        _state,
        _time,
    ):
        sock = Mock()
        controlled_links = [
            (("incoming", "outgoing", "via"),),
        ]

        count = bridge.publish_traffic_light_updates(
            sock,
            "0",
            controlled_links,
        )

        self.assertEqual(count, 1)
        sock.sendto.assert_called_once_with(
            b"SUMO_TLS,0,0,G,2,8.000000,6.000000,0,1.500000",
            ("127.0.0.1", 7400),
        )


    def test_continues_tls_simulation_without_expected_vehicles(self):
        self.assertTrue(
            bridge.should_continue_simulation(
                min_expected_number=0,
                tls_enabled=True,
                sim_time=0.0,
                sim_time_limit=30.0,
            )
        )

    def test_stops_tls_only_simulation_at_time_limit(self):
        self.assertFalse(
            bridge.should_continue_simulation(
                min_expected_number=0,
                tls_enabled=True,
                sim_time=30.0,
                sim_time_limit=30.0,
            )
        )

    def test_stops_at_time_limit_even_with_expected_vehicles(self):
        self.assertFalse(
            bridge.should_continue_simulation(
                min_expected_number=1,
                tls_enabled=True,
                sim_time=31.0,
                sim_time_limit=30.0,
            )
        )

    def test_preserves_vehicle_driven_loop_without_tls(self):
        self.assertTrue(
            bridge.should_continue_simulation(
                min_expected_number=1,
                tls_enabled=False,
                sim_time=31.0,
                sim_time_limit=30.0,
            )
        )

    def test_stops_when_no_tls_and_no_expected_vehicles(self):
        self.assertFalse(
            bridge.should_continue_simulation(
                min_expected_number=0,
                tls_enabled=False,
                sim_time=0.0,
                sim_time_limit=30.0,
            )
        )


if __name__ == "__main__":
    unittest.main()
