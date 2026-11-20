#!/usr/bin/env python3
"""Source contract for the shared WGS84 position resolution path."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def source(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def strip_comments(value):
    value = re.sub(r"/\*.*?\*/", "", value, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", value)


class TestUnifiedGeodeticPositionContract(unittest.TestCase):
    def test_no_cartesian_metres_are_approximated_as_degrees(self):
        denm = source(
            "scratch/vanetza_integration/denm/builder/denm_builder.cc"
        )
        geonet = source(
            "scratch/vanetza_integration/wrappers/vanetza/position/"
            "ns3_position_provider.cc"
        )
        for path, content in (
            ("DENM builder", denm),
            ("GeoNetworking provider", geonet),
        ):
            self.assertNotIn("111320.0", content, path)
            self.assertNotIn("meters_per_degree", content, path)

    def test_cam_denm_and_geonet_use_the_common_resolver(self):
        cam = strip_comments(source(
            "scratch/vanetza_integration/cam/builder/cam_builder.cc"
        ))
        denm = strip_comments(source(
            "scratch/vanetza_integration/denm/builder/denm_builder.cc"
        ))
        geonet = strip_comments(source(
            "scratch/vanetza_integration/wrappers/vanetza/position/"
            "ns3_position_provider.cc"
        ))
        for label, content in (
            ("CAM", cam),
            ("DENM", denm),
            ("GeoNetworking", geonet),
        ):
            self.assertIn("SumoGeodeticTransform::ResolveWgs84", content, label)

    def test_projection_configuration_is_shared_with_simulation(self):
        header = source(
            "scratch/vanetza_integration/context/simulation_context/"
            "simulation_context.h"
        )
        implementation = source(
            "scratch/vanetza_integration/context/simulation_context/"
            "simulation_context.cc"
        )
        main = source("scratch/v2x/v2x_sumo.cc")
        self.assertIn("sumoProjection", header)
        self.assertIn("sumoProjection", implementation)
        self.assertIn("SimulationContext::sumoProjection = sumoProjection", main)

    def test_denm_event_preserves_geodetic_position(self):
        event_header = source(
            "scratch/vanetza_integration/denm/scenario/"
            "denm_scenario_event_source.h"
        )
        trigger_header = source(
            "scratch/vanetza_integration/denm/trigger/denm_trigger.h"
        )
        event_source = source(
            "scratch/vanetza_integration/denm/scenario/"
            "denm_scenario_event_source.cc"
        )
        controller = strip_comments(source(
            "scratch/vanetza_integration/context/simulation_controller/"
            "simulation_controller.cc"
        ))
        self.assertIn("std::optional<Wgs84Position> geodeticPosition", event_header)
        self.assertIn("std::optional<Wgs84Position> geodeticPosition", trigger_header)
        self.assertIn(
            "request.geodeticPosition = scheduled.event.geodeticPosition",
            event_source,
        )
        self.assertIn("SumoGeodeticTransform::ResolveWgs84", controller)
        self.assertIn("resolvedEvent.geodeticPosition = resolvedGeodetic", controller)


if __name__ == "__main__":
    unittest.main(verbosity=2)
