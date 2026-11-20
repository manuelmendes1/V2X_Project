#!/usr/bin/env python3

import inspect
import math
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, call, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import v2x_bridge_16 as bridge


class TestV2XBridge16VehicleLifecycle(unittest.TestCase):
    def require_callable(self, name):
        candidate = getattr(bridge, name, None)
        self.assertTrue(
            callable(candidate),
            f"SUMO_VEHICLE_LIFECYCLE_API_MISSING: {name}",
        )

    def test_serializes_vehicle_remove(self):
        self.require_callable("serialize_vehicle_remove")
        self.assertEqual(
            bridge.serialize_vehicle_remove("veh0", 1234567),
            "SUMO_VEHICLE,REMOVE,veh0,1234567",
        )

    def test_rejects_invalid_vehicle_remove_identity_and_timestamp(self):
        self.require_callable("serialize_vehicle_remove")

        for vehicle_id in ("", "veh,0", "veh\n0"):
            with self.subTest(vehicle_id=vehicle_id):
                with self.assertRaises(ValueError):
                    bridge.serialize_vehicle_remove(vehicle_id, 10)

        for timestamp in (-1, 1.5, True, 1 << 64):
            with self.subTest(timestamp=timestamp):
                with self.assertRaises(ValueError):
                    bridge.serialize_vehicle_remove("veh0", timestamp)

    def test_sends_explicit_vehicle_remove(self):
        self.require_callable("send_vehicle_removal_to_ns3")
        sock = Mock()

        bridge.send_vehicle_removal_to_ns3(sock, "veh0", 1234567)

        sock.sendto.assert_called_once_with(
            b"SUMO_VEHICLE,REMOVE,veh0,1234567",
            ("127.0.0.1", 7400),
        )

    def test_publishes_active_vehicles_and_departures(self):
        self.require_callable("publish_vehicle_updates")
        sock = Mock()
        previous = {"veh0", "veh-old"}
        current = ["veh1", "veh0"]

        with (
            patch.object(bridge, "send_vehicle_to_ns3") as send_upsert,
            patch.object(
                bridge, "send_vehicle_removal_to_ns3"
            ) as send_remove,
        ):
            result = bridge.publish_vehicle_updates(
                sock, current, previous, 1234567
            )

        self.assertEqual(result, {"veh0", "veh1"})
        self.assertEqual(
            send_upsert.call_args_list,
            [
                call(sock, "veh0", 1234567),
                call(sock, "veh1", 1234567),
            ],
        )
        send_remove.assert_called_once_with(sock, "veh-old", 1234567)

    def test_temporarily_unavailable_sample_does_not_remove_vehicle(self):
        self.require_callable("publish_vehicle_updates")
        sock = Mock()

        def send_sample(_sock, vehicle_id, _timestamp):
            if vehicle_id == "veh-unavailable":
                raise ValueError("temporary TraCI sample unavailable")

        with (
            patch.object(
                bridge, "send_vehicle_to_ns3", side_effect=send_sample
            ),
            patch.object(
                bridge, "send_vehicle_removal_to_ns3"
            ) as send_remove,
        ):
            result = bridge.publish_vehicle_updates(
                sock,
                ["veh-unavailable"],
                {"veh-unavailable", "veh-departed"},
                200,
            )

        self.assertEqual(result, {"veh-unavailable"})
        send_remove.assert_called_once_with(sock, "veh-departed", 200)

    def test_first_snapshot_does_not_infer_departures(self):
        self.require_callable("publish_vehicle_updates")

        with (
            patch.object(bridge, "send_vehicle_to_ns3"),
            patch.object(
                bridge, "send_vehicle_removal_to_ns3"
            ) as send_remove,
        ):
            result = bridge.publish_vehicle_updates(
                Mock(), ["veh0"], set(), 10
            )

        self.assertEqual(result, {"veh0"})
        send_remove.assert_not_called()

    def test_main_publishes_vehicle_snapshot_before_step_end(self):
        source = inspect.getsource(bridge.main)

        self.assertIn("traci.vehicle.getIDList()", source)
        self.assertIn("previous_vehicle_ids", source)
        self.assertIn("publish_vehicle_updates(", source)

        loop_start = source.find("while should_continue_simulation_us(")
        self.assertGreaterEqual(loop_start, 0)

        publication = source.find("publish_vehicle_updates(", loop_start)
        marker = source.find("step_end_message", loop_start)

        self.assertGreater(publication, loop_start)
        self.assertGreater(marker, publication)

    def test_main_flushes_vehicle_departures_after_loop(self):
        source = inspect.getsource(bridge.main)
        lines = source.splitlines()

        while_index = next(
            index
            for index, line in enumerate(lines)
            if line.strip().startswith("while should_continue_simulation_us")
        )
        loop_indent = len(lines[while_index]) - len(lines[while_index].lstrip())
        header_end = next(
            index
            for index in range(while_index, len(lines))
            if lines[index].strip().endswith("):")
        )
        shutdown_index = next(
            index
            for index in range(header_end + 1, len(lines))
            if lines[index].lstrip().startswith("except KeyboardInterrupt")
        )

        post_loop_start = None
        for index in range(header_end + 1, shutdown_index):
            line = lines[index]
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            indent = len(line) - len(line.lstrip())
            if indent <= loop_indent:
                post_loop_start = index
                break

        self.assertIsNotNone(post_loop_start)
        post_loop = "\n".join(lines[post_loop_start:shutdown_index])

        self.assertIn("publish_vehicle_updates(", post_loop)
        self.assertIn("previous_vehicle_ids", post_loop)
        self.assertIn("SUMO_STEP_END", post_loop)


if __name__ == "__main__":
    unittest.main(verbosity=2)
