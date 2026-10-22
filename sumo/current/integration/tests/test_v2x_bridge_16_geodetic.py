#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import v2x_bridge_16 as bridge


class TestV2XBridge16Geodetic(unittest.TestCase):

    def send_sample(self, convert_geo_result=None, convert_geo_error=None):
        sock = Mock()

        with (
            patch.object(
                bridge.traci.simulation, "getTime",
                return_value=1.234567,
            ),
            patch.object(
                bridge.traci.vehicle, "getPosition",
                return_value=(20489.043, 5179.750),
            ),
            patch.object(
                bridge.traci.vehicle, "getSpeed", return_value=13.0
            ),
            patch.object(
                bridge.traci.vehicle, "getAngle", return_value=90.0
            ),
            patch.object(
                bridge.traci.vehicle, "getAcceleration",
                return_value=0.25,
            ),
            patch.object(
                bridge.traci.simulation, "convertGeo",
                return_value=convert_geo_result,
                side_effect=convert_geo_error,
            ) as convert_geo,
        ):
            bridge.send_vehicle_to_ns3(sock, "veh0")

        payload = sock.sendto.call_args.args[0].decode("utf-8")
        return payload, convert_geo

    def test_valid_geodetic_conversion_emits_nine_fields(self):
        message, convert_geo = self.send_sample(
            convert_geo_result=(-8.4330521, 41.3851057)
        )
        fields = message.split(",")

        convert_geo.assert_called_once_with(20489.043, 5179.750)
        self.assertEqual(len(fields), 9)
        self.assertEqual(fields[0], "veh0")
        self.assertAlmostEqual(float(fields[1]), 20489.043, places=3)
        self.assertAlmostEqual(float(fields[2]), 5179.750, places=3)
        self.assertEqual(fields[6], "1234567")
        self.assertAlmostEqual(float(fields[7]), -8.4330521, places=6)
        self.assertAlmostEqual(float(fields[8]), 41.3851057, places=6)

    def test_conversion_exception_keeps_timestamp_without_wgs84(self):
        message, convert_geo = self.send_sample(
            convert_geo_error=RuntimeError("conversion unavailable")
        )
        fields = message.split(",")

        convert_geo.assert_called_once_with(20489.043, 5179.750)
        self.assertEqual(len(fields), 7)
        self.assertEqual(fields[0], "veh0")
        self.assertEqual(fields[6], "1234567")

    def test_out_of_range_conversion_keeps_timestamp_without_wgs84(self):
        message, convert_geo = self.send_sample(
            convert_geo_result=(181.0, 41.0)
        )
        fields = message.split(",")

        convert_geo.assert_called_once_with(20489.043, 5179.750)
        self.assertEqual(len(fields), 7)
        self.assertEqual(fields[6], "1234567")


if __name__ == "__main__":
    unittest.main(verbosity=2)
