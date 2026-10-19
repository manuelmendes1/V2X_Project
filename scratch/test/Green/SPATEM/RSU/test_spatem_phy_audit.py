import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]

HEADER = ROOT / "scratch/vanetza_integration/core/phy_audit/phy_audit.h"
RSU_BUILDER = ROOT / "scratch/vanetza_integration/spatem/emitter/its_station_network_builder.cc"
VEHICLE_BUILDER = ROOT / "scratch/vanetza_integration/context/vehicle_network/vehicle_network_builder.cc"


class SpatemPhyAuditTest(unittest.TestCase):
    def test_audit_helper_is_opt_in(self):
        self.assertTrue(
            HEADER.is_file(),
            f"Missing PHY audit helper: {HEADER}",
        )
        source = HEADER.read_text()
        self.assertIn("V2X_PHY_AUDIT", source)
        self.assertIn("getenv", source)

    def test_helper_supports_required_physical_traces(self):
        source = HEADER.read_text() if HEADER.is_file() else ""
        for trace in (
            "PhyTxBegin",
            "PhyTxEnd",
            "PhyRxBegin",
            "PhyRxEnd",
            "PhyRxDrop",
        ):
            with self.subTest(trace=trace):
                self.assertIn(trace, source)

        self.assertIn("PacketIdTag", source)
        self.assertIn("GetPacketId()", source)

    def test_rsu_network_connects_transmit_traces(self):
        source = RSU_BUILDER.read_text()
        self.assertIn("PhyAudit::ConnectRsu(node, devices)", source)

    def test_vehicle_network_connects_receive_traces(self):
        source = VEHICLE_BUILDER.read_text()
        self.assertIn("PhyAudit::ConnectVehicle(node, devices)", source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
