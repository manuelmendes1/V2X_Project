from pathlib import Path
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("ns-3 repository root not found")


class TestVehicleIdentityAuthorityWiring(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        root = repository_root(Path(__file__))
        cls.source_path = (
            root / "scratch/vanetza_integration/context/"
            "vehicle_identity/vehicle_identity_initializer.cc"
        )
        cls.source = cls.source_path.read_text()

    def test_vehicle_identity_uses_shared_station_id_authority(self):
        self.assertIn(
            "context/its_station_identity/"
            "its_station_identity_authority.h",
            self.source,
            "VEHICLE_IDENTITY_AUTHORITY_INCLUDE_MISSING",
        )

        self.assertIn(
            "ItsStationIdentityAuthority::Instance()",
            self.source,
            "VEHICLE_IDENTITY_DOES_NOT_USE_SHARED_AUTHORITY",
        )

        self.assertIn(
            "ItsStationIdentityOwnerKind::Vehicle",
            self.source,
            "VEHICLE_IDENTITY_OWNER_KIND_MISSING",
        )

        self.assertIn(
            "state.identity.vehId",
            self.source,
            "VEHICLE_LOGICAL_OWNER_ID_MISSING",
        )

        self.assertIn(
            "ReserveNodeDerived(",
            self.source,
            "VEHICLE_STATION_ID_NOT_RESERVED_GLOBALLY",
        )

    def test_reservation_precedes_identity_assignment_and_reverse_indexes(self):
        reserve = self.source.find("ReserveNodeDerived(")
        assignment = self.source.find(
            "state.identity.stationId =",
            reserve,
        )
        node_index = self.source.find("registry.RegisterNodeId(")
        station_index = self.source.find("registry.RegisterStationId(")

        self.assertGreaterEqual(
            reserve, 0, "VEHICLE_STATION_ID_RESERVATION_MISSING"
        )
        self.assertGreater(
            assignment, reserve,
            "STATION_ID_ASSIGNED_BEFORE_AUTHORITY_RESERVATION",
        )
        self.assertGreater(
            node_index, assignment,
            "NODE_REVERSE_INDEX_REGISTERED_BEFORE_IDENTITY",
        )
        self.assertGreater(
            station_index, assignment,
            "STATION_REVERSE_INDEX_REGISTERED_BEFORE_IDENTITY",
        )

    def test_vehicle_identity_does_not_derive_station_id_outside_authority(self):
        self.assertNotIn(
            "state.identity.nodeId + 1u",
            self.source,
            "VEHICLE_STATION_ID_DERIVATION_BYPASSES_AUTHORITY",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
