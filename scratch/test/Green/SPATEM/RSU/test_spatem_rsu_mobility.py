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
INTEGRATION = REPO_ROOT / "scratch/vanetza_integration"
NETWORK_BUILDER = (
    INTEGRATION
    / "context/its_station/network/its_station_network_builder.cc"
)
SPATEM_ENDPOINT = (
    INTEGRATION / "spatem/emitter/spatem_emitter_endpoint.cc"
)


class TestSpatemRsuMobility(unittest.TestCase):
    def test_network_builder_preserves_caller_mobility_before_wifi(self):
        source = NETWORK_BUILDER.read_text(encoding="utf-8")

        self.assertIn(
            '#include "ns3/mobility-model.h"',
            source,
            "Neutral ITS station network builder must use MobilityModel",
        )

        start_marker = (
            "void\nItsStationNetworkBuilder::ConfigureNetworking("
        )
        end_marker = "\nPtr<Socket>\nItsStationNetworkBuilder::CreateSocket("
        self.assertIn(start_marker, source)
        self.assertIn(end_marker, source)
        section = source.split(start_marker, 1)[1].split(end_marker, 1)[0]

        self.assertIn("config.mobilityInstaller(node)", section)
        self.assertIn("node->GetObject<MobilityModel>()", section)
        self.assertIn("mobility->SetPosition(config.initialPosition)", section)

        mobility_install = section.find("config.mobilityInstaller(node)")
        wifi_install = section.find("wifi.Install(")
        self.assertGreaterEqual(mobility_install, 0)
        self.assertGreaterEqual(wifi_install, 0)
        self.assertLess(
            mobility_install,
            wifi_install,
            "Mobility must be installed before the Wi-Fi device",
        )

    def test_spatem_rsu_selects_constant_position_mobility_explicitly(self):
        source = SPATEM_ENDPOINT.read_text(encoding="utf-8")
        self.assertIn('#include "ns3/mobility-module.h"', source)
        self.assertIn(
            'mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel")',
            source,
        )
        self.assertIn("networkConfig.phyAuditRole = PhyAuditRole::Rsu", source)
        self.assertIn("networkConfig.mobilityInstaller =", source)
        self.assertIn("ItsStationNetworkBuilder::Build(", source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
