#!/usr/bin/env python3

import re
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


REPO_ROOT = find_repository_root(Path(__file__))
SOURCE = (
    REPO_ROOT
    / "scratch/vanetza_integration/spatem/emitter/"
      "its_station_network_builder.cc"
)


class TestSpatemRsuMobility(unittest.TestCase):
    def test_constant_position_mobility_is_installed_before_wifi(self):
        source = SOURCE.read_text()

        self.assertIn(
            '#include "ns3/mobility-module.h"',
            source,
            "RSU network builder must include the ns-3 mobility module",
        )

        start_marker = (
            "void\nItsStationNetworkBuilder::ConfigureNetworking("
        )
        end_marker = (
            "\nPtr<Socket>\nItsStationNetworkBuilder::CreateSocket("
        )

        self.assertIn(start_marker, source)
        self.assertIn(end_marker, source)

        section = source.split(start_marker, 1)[1].split(end_marker, 1)[0]

        self.assertRegex(
            section,
            re.compile(
                r'MobilityHelper\s+\w+\s*;'
                r'[\s\S]*?SetMobilityModel\s*\('
                r'\s*"ns3::ConstantPositionMobilityModel"\s*\)'
                r'[\s\S]*?\.Install\s*\(\s*node\s*\)\s*;',
            ),
            "RSU node must install ConstantPositionMobilityModel",
        )

        mobility_install = section.find(".Install(node)")
        wifi_install = section.find("wifi.Install(")

        self.assertGreaterEqual(mobility_install, 0)
        self.assertGreaterEqual(wifi_install, 0)
        self.assertLess(
            mobility_install,
            wifi_install,
            "Mobility must be installed before the Wi-Fi device",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
