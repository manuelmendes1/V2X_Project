from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[4]


def read(relative_path):
    return (ROOT / relative_path).read_text(encoding="utf-8")


def extract_function(source, signature):
    start = source.find(signature)
    if start < 0:
        return ""

    opening = source.find("{", start)
    if opening < 0:
        return ""

    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]

    return ""


class TestStationEndpointRegistryAuthorityWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.registry = read(
            "scratch/vanetza_integration/context/"
            "station_registry/station_endpoint_registry.h"
        )

        cls.register_vehicle = extract_function(
            cls.registry, "RegisterVehicle("
        )
        cls.register_its_station = extract_function(
            cls.registry, "RegisterItsStation("
        )
        cls.unregister_its_station = extract_function(
            cls.registry, "UnregisterItsStation("
        )

    def test_registry_includes_global_identity_authority(self):
        self.assertIn(
            "context/its_station_identity/"
            "its_station_identity_authority.h",
            self.registry,
            "STATION_REGISTRY_AUTHORITY_INCLUDE_MISSING",
        )

    def test_vehicle_registration_requires_matching_reservation(self):
        body = self.register_vehicle
        self.assertTrue(body, "REGISTER_VEHICLE_NOT_FOUND")

        check = body.find("IsReservedBy(")
        register = body.find("return Register(std::move(identity))")

        self.assertGreaterEqual(
            check, 0, "VEHICLE_STATION_RESERVATION_CHECK_MISSING"
        )
        self.assertGreater(
            register, check,
            "VEHICLE_REGISTERED_BEFORE_RESERVATION_VALIDATION",
        )
        self.assertIn(
            "ItsStationIdentityOwnerKind::Vehicle",
            body,
            "VEHICLE_RESERVATION_OWNER_KIND_INVALID",
        )
        self.assertIn(
            "vehicleId",
            body,
            "VEHICLE_RESERVATION_OWNER_ID_MISSING",
        )
        self.assertIn(
            "stationId",
            body[check:register],
            "VEHICLE_RESERVATION_STATION_ID_NOT_VERIFIED",
        )
        self.assertIn(
            "return false;",
            body[check:register],
            "UNRESERVED_VEHICLE_MUST_BE_REJECTED",
        )

    def test_its_station_registration_requires_matching_reservation(self):
        body = self.register_its_station
        self.assertTrue(body, "REGISTER_ITS_STATION_NOT_FOUND")

        check = body.find("IsReservedBy(")
        register = body.find("return Register(std::move(identity))")

        self.assertGreaterEqual(
            check, 0, "ITS_STATION_RESERVATION_CHECK_MISSING"
        )
        self.assertGreater(
            register, check,
            "ITS_STATION_REGISTERED_BEFORE_RESERVATION_VALIDATION",
        )
        self.assertIn(
            "ItsStationIdentityOwnerKind::ItsStation",
            body,
            "ITS_STATION_RESERVATION_OWNER_KIND_INVALID",
        )
        self.assertIn(
            "logicalId",
            body,
            "ITS_STATION_RESERVATION_OWNER_ID_MISSING",
        )
        self.assertIn(
            "stationId",
            body[check:register],
            "ITS_STATION_RESERVATION_STATION_ID_NOT_VERIFIED",
        )
        self.assertIn(
            "return false;",
            body[check:register],
            "UNRESERVED_ITS_STATION_MUST_BE_REJECTED",
        )

    def test_ipv4_unregister_does_not_release_logical_identity(self):
        body = self.unregister_its_station
        self.assertTrue(body, "UNREGISTER_ITS_STATION_NOT_FOUND")
        self.assertNotIn(
            ".Release(",
            body,
            "IPV4_UNREGISTER_MUST_NOT_RELEASE_GLOBAL_STATION_ID",
        )

    def test_existing_ipv4_index_and_collision_guard_are_preserved(self):
        self.assertIn(
            "m_byIpv4.find(address)",
            self.registry,
            "IPV4_LOOKUP_INDEX_MISSING",
        )
        self.assertIn(
            "current.stationId == identity.stationId",
            self.registry,
            "SAME_ENDPOINT_IDEMPOTENCE_GUARD_MISSING",
        )
        self.assertIn(
            "m_byIpv4.emplace(",
            self.registry,
            "IPV4_ENDPOINT_INSERTION_MISSING",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
