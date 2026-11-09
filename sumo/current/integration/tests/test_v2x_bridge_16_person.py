#!/usr/bin/env python3

import inspect
import math
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import v2x_bridge_16 as bridge


class TestV2XBridge16Person(unittest.TestCase):
    def require_callable(self, name):
        candidate = getattr(bridge, name, None)
        self.assertTrue(
            callable(candidate),
            f"SUMO_PERSON_API_MISSING: {name} ainda não está implementada",
        )

    def test_serializes_person_upsert_using_cpp_contract(self):
        self.require_callable("serialize_person_upsert")
        message = bridge.serialize_person_upsert(
            person_id="ped0",
            x=10.125,
            y=20.250,
            speed=1.4,
            angle_deg=90.0,
            simulation_time_us=1234567,
        )
        self.assertEqual(
            message,
            "SUMO_PERSON,UPSERT,ped0,10.125,20.250,1.400,90.000,1234567",
        )

    def test_serializes_explicit_person_removal(self):
        self.require_callable("serialize_person_remove")
        self.assertEqual(
            bridge.serialize_person_remove("ped0", 1234568),
            "SUMO_PERSON,REMOVE,ped0,1234568",
        )

    def test_rejects_empty_or_csv_ambiguous_person_ids(self):
        self.require_callable("serialize_person_upsert")
        for person_id in ("", "ped,0"):
            with self.subTest(person_id=person_id):
                with self.assertRaises(ValueError):
                    bridge.serialize_person_upsert(
                        person_id, 1.0, 2.0, 1.0, 90.0, 10
                    )

    def test_rejects_non_finite_person_samples(self):
        self.require_callable("serialize_person_upsert")
        invalid_samples = (
            (math.nan, 2.0, 1.0, 90.0),
            (1.0, math.inf, 1.0, 90.0),
            (1.0, 2.0, math.nan, 90.0),
            (1.0, 2.0, 1.0, math.inf),
        )
        for sample in invalid_samples:
            with self.subTest(sample=sample):
                with self.assertRaises(ValueError):
                    bridge.serialize_person_upsert("ped0", *sample, 10)

    def test_rejects_negative_speed(self):
        self.require_callable("serialize_person_upsert")
        with self.assertRaises(ValueError):
            bridge.serialize_person_upsert("ped0", 1.0, 2.0, -0.1, 90.0, 10)

    def test_rejects_traci_unavailable_value_sentinels(self):
        self.require_callable("serialize_person_upsert")
        unavailable = -1073741824.0
        invalid_samples = (
            (unavailable, unavailable, 1.0, 90.0),
            (1.0, 2.0, unavailable, 90.0),
            (1.0, 2.0, 1.0, unavailable),
        )
        for sample in invalid_samples:
            with self.subTest(sample=sample):
                with self.assertRaises(ValueError):
                    bridge.serialize_person_upsert("ped0", *sample, 10)

    def test_requires_unsigned_integer_simulation_timestamp(self):
        self.require_callable("serialize_person_upsert")
        for timestamp in (-1, 1.5, True, 1 << 64):
            with self.subTest(timestamp=timestamp):
                with self.assertRaises(ValueError):
                    bridge.serialize_person_upsert(
                        "ped0", 1.0, 2.0, 1.0, 90.0, timestamp
                    )

    @patch.object(
        bridge.traci.vehicletype, "getVehicleClass", return_value="pedestrian"
    )
    @patch.object(bridge.traci.person, "getAngle", return_value=90.0)
    @patch.object(bridge.traci.person, "getSpeed", return_value=1.4)
    @patch.object(
        bridge.traci.person, "getPosition", return_value=(10.125, 20.250)
    )
    def test_reads_person_domain_and_sends_upsert(
        self, get_position, get_speed, get_angle, get_vehicle_class
    ):
        self.require_callable("send_person_to_ns3")
        sock = Mock()
        bridge.send_person_to_ns3(sock, "ped0", 1234567)

        get_position.assert_called_once_with("ped0")
        get_speed.assert_called_once_with("ped0")
        get_angle.assert_called_once_with("ped0")
        get_vehicle_class.assert_called_once_with("ped0")
        sock.sendto.assert_called_once_with(
            b"SUMO_PERSON,UPSERT,ped0,10.125,20.250,1.400,90.000,1234567,pedestrian",
            ("127.0.0.1", 7400),
        )

    def test_publishes_active_persons_and_explicit_removals(self):
        self.require_callable("send_person_to_ns3")
        self.require_callable("send_person_removal_to_ns3")
        self.require_callable("publish_person_updates")
        sock = Mock()
        previous = {"ped0", "ped_old"}
        current = ["ped1", "ped0"]

        with (
            patch.object(bridge, "send_person_to_ns3") as send_upsert,
            patch.object(
                bridge, "send_person_removal_to_ns3"
            ) as send_remove,
        ):
            result = bridge.publish_person_updates(
                sock, current, previous, 1234567
            )

        self.assertEqual(result, {"ped0", "ped1"})
        self.assertEqual(
            send_upsert.call_args_list,
            [
                unittest.mock.call(sock, "ped0", 1234567),
                unittest.mock.call(sock, "ped1", 1234567),
            ],
        )
        send_remove.assert_called_once_with(sock, "ped_old", 1234567)

    def test_no_absence_based_removal_without_previous_membership(self):
        self.require_callable("send_person_to_ns3")
        self.require_callable("send_person_removal_to_ns3")
        self.require_callable("publish_person_updates")
        sock = Mock()
        with (
            patch.object(bridge, "send_person_to_ns3"),
            patch.object(bridge, "send_person_removal_to_ns3") as send_remove,
        ):
            bridge.publish_person_updates(sock, [], set(), 10)
        send_remove.assert_not_called()

    def test_main_publishes_person_records_before_step_end(self):
        source = inspect.getsource(bridge.main)
        self.assertIn("traci.person.getIDList()", source)
        self.assertIn("previous_person_ids", source)
        self.assertIn("publish_person_updates(", source)
        self.assertLess(
            source.index("publish_person_updates("),
            source.index("step_end_message"),
        )

    def test_main_flushes_remaining_persons_after_loop_termination(self):
        source = inspect.getsource(bridge.main)
        lines = source.splitlines()
        while_index = next(
            index
            for index, line in enumerate(lines)
            if line.strip().startswith("while should_continue_simulation")
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

        self.assertIsNotNone(
            post_loop_start,
            "O bridge não tem uma fase de drenagem após o ciclo SUMO",
        )
        post_loop = "\n".join(lines[post_loop_start:shutdown_index])
        self.assertIn(
            "publish_person_updates(",
            post_loop,
            "Ao terminar, o bridge tem de reconciliar o snapshot final de pessoas",
        )
        self.assertIn(
            "previous_person_ids",
            post_loop,
            "A drenagem final tem de remover IDs que ainda estavam registados",
        )
        self.assertIn(
            "SUMO_STEP_END",
            post_loop,
            "Os REMOVE finais têm de ser delimitados por SUMO_STEP_END",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
