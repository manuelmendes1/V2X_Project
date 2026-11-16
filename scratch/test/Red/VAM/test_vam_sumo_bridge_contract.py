import importlib.util
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
FIXTURE_ROOT = ROOT / "scratch/test/fixtures/vam_sumo"
SCENARIO_DIR = FIXTURE_ROOT / "scenario/vam"
BRIDGE = FIXTURE_ROOT / "integration/v2x_bridge_vam.py"

SUBJECTS_H = ROOT / "scratch/vanetza_integration/vam/scenario/vam_scenario_subjects.h"
SUBJECTS_CC = ROOT / "scratch/vanetza_integration/vam/scenario/vam_scenario_subjects.cc"
MAIN = ROOT / "scratch/v2x/v2x_sumo.cc"
CMAKE = ROOT / "scratch/CMakeLists.txt"


class TestVamScenarioModuleWiring(unittest.TestCase):
    def test_scenario_subject_module_is_declared_and_implemented(self):
        self.assertTrue(SUBJECTS_H.is_file(), "VAM_SCENARIO_HEADER_MISSING")
        self.assertTrue(SUBJECTS_CC.is_file(), "VAM_SCENARIO_SOURCE_MISSING")

        header = SUBJECTS_H.read_text()
        source = SUBJECTS_CC.read_text()

        self.assertIn("MakeVamEvaluationScenarioSubjects", header)
        self.assertIn("MakeVamEvaluationScenarioSubjects", source)

    def test_scenario_uses_an_explicit_synthetic_identity(self):
        self.assertTrue(SUBJECTS_CC.is_file(), "VAM_SCENARIO_SOURCE_MISSING")
        source = SUBJECTS_CC.read_text()

        self.assertIn("vam-eval-ego-001", source)
        self.assertIn("0x70000001U", source)
        self.assertIn("VbsState::ActiveStandalone", source)
        self.assertIn("synthetic", source.lower())

    def test_main_uses_scenario_subjects_only_when_vam_is_enabled(self):
        main = MAIN.read_text()

        self.assertIn("vam_scenario_subjects.h", main)
        self.assertIn("MakeVamEvaluationScenarioSubjects()", main)

        parse_position = main.find("cmd.Parse(argc, argv)")
        factory_position = main.find("MakeVamEvaluationScenarioSubjects()")

        self.assertGreaterEqual(parse_position, 0)
        self.assertGreater(
            factory_position,
            parse_position,
            "Scenario identities must be selected after CLI parsing",
        )
        self.assertIn("bool enableVamEvaluationScenario = false", main)
        self.assertIn('"enableVamEvaluationScenario"', main)
        self.assertIn(
            "if (enableVamGeneration && enableVamEvaluationScenario)",
            main,
        )
        self.assertIn(
            "if (enableVamEvaluationScenario && !enableVamGeneration)",
            main,
        )

    def test_scenario_source_belongs_to_existing_vam_object_library(self):
        cmake = CMAKE.read_text()
        self.assertIn(
            "vanetza_integration/vam/scenario/vam_scenario_subjects.cc",
            cmake,
        )

    def test_scenario_files_reuse_existing_network_and_vehicle_routes(self):
        config_path = SCENARIO_DIR / "vam.sumocfg"
        routes_path = SCENARIO_DIR / "vam.rou.xml"
        net_path = SCENARIO_DIR.parent / "simple.net.xml"
        vehicle_routes_path = SCENARIO_DIR.parent / "simple.rou.xml"

        for path in (config_path, routes_path, net_path, vehicle_routes_path):
            self.assertTrue(path.is_file(), f"SCENARIO_FILE_MISSING: {path}")

        config = ET.parse(config_path).getroot()
        net_element = config.find("./input/net-file")
        route_element = config.find("./input/route-files")

        self.assertIsNotNone(net_element, "SUMO_NET_FILE_ELEMENT_MISSING")
        self.assertIsNotNone(
            route_element,
            "SUMO_ROUTE_FILES_ELEMENT_MISSING",
        )

        net_value = net_element.get("value")
        route_value = route_element.get("value")

        self.assertEqual(net_value, "../simple.net.xml")
        self.assertIsNotNone(route_value)

        configured_net = (
            config_path.parent / net_value.strip()
        ).resolve()
        self.assertEqual(configured_net, net_path.resolve())

        configured_routes = [
            (config_path.parent / value.strip()).resolve()
            for value in route_value.split(",")
        ]
        self.assertIn(vehicle_routes_path.resolve(), configured_routes)
        self.assertIn(routes_path.resolve(), configured_routes)

    def test_scenario_declares_ego_and_peer_as_sumo_pedestrians(self):
        routes_path = SCENARIO_DIR / "vam.rou.xml"
        self.assertTrue(routes_path.is_file(), "VAM_ROUTES_MISSING")

        routes = ET.parse(routes_path).getroot()
        persons = {
            person.attrib.get("id"): person
            for person in routes.findall("person")
        }

        self.assertIn("vam-eval-ego-001", persons)
        self.assertIn("vam-eval-peer-001", persons)

        for person in persons.values():
            walk = person.find("walk")
            self.assertIsNotNone(walk)
            self.assertTrue(walk.attrib.get("edges"))


class FakeSocket:
    def __init__(self):
        self.messages = []
        self.destinations = []

    def sendto(self, data, destination):
        self.messages.append(data.decode("utf-8"))
        self.destinations.append(destination)


class FakeSimulation:
    @staticmethod
    def getTime():
        return 0.1


class FakeVehicle:
    @staticmethod
    def getIDList():
        return ["veh0"]

    @staticmethod
    def getPosition(_vehicle_id):
        return (10.0, 20.0)

    @staticmethod
    def getSpeed(_vehicle_id):
        return 13.9

    @staticmethod
    def getAngle(_vehicle_id):
        return 90.0

    @staticmethod
    def getAcceleration(_vehicle_id):
        return 0.0


class FakePerson:
    @staticmethod
    def getIDList():
        return ["vam-eval-ego-001"]

    @staticmethod
    def getPosition(_person_id):
        return (11.0, 20.0)

    @staticmethod
    def getSpeed(_person_id):
        return 1.4

    @staticmethod
    def getAngle(_person_id):
        return 90.0

    @staticmethod
    def getVehicleClass(_person_id):
        return "pedestrian"


class FakeTraCI:
    simulation = FakeSimulation()
    vehicle = FakeVehicle()
    person = FakePerson()


class TestVamSumoBridgeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not BRIDGE.is_file():
            cls.bridge = None
            return

        spec = importlib.util.spec_from_file_location(
            "v2x_bridge_vam_contract_test",
            BRIDGE,
        )
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.traci = FakeTraCI()
        cls.bridge = module

    def setUp(self):
        if self.bridge is None:
            return

        # A fresh person API prevents one test's REMOVE fixture from
        # mutating the TraCI fake used by subsequent tests.
        person_api = type("IsolatedPerson", (FakePerson,), {})
        traci_api = type(
            "IsolatedTraCI",
            (),
            {
                "simulation": FakeSimulation(),
                "vehicle": FakeVehicle(),
                "person": person_api,
            },
        )
        self.bridge.traci = traci_api

    def require_bridge(self):
        self.assertIsNotNone(self.bridge, "VAM_SUMO_BRIDGE_MISSING")
        return self.bridge

    def test_step_end_is_last_datagram_after_vehicle_and_person_updates(self):
        bridge = self.require_bridge()
        sock = FakeSocket()

        current_vehicles, current_persons = (
            bridge.publish_current_step(sock, set(), set())
        )

        self.assertEqual(current_vehicles, {"veh0"})
        self.assertEqual(current_persons, {"vam-eval-ego-001"})
        self.assertTrue(sock.messages[0].startswith("veh0,"))
        self.assertTrue(
            sock.messages[1].startswith("SUMO_PERSON,UPSERT,")
        )
        self.assertEqual(sock.messages[-1], "SUMO_STEP_END,100000")

    def test_person_remove_is_sent_before_step_end(self):
        bridge = self.require_bridge()
        sock = FakeSocket()

        bridge.traci.person.getIDList = staticmethod(lambda: [])

        bridge.publish_current_step(
            sock,
            previous_vehicle_ids={"veh0"},
            previous_person_ids={"vam-eval-ego-001"},
        )

        self.assertIn(
            "SUMO_PERSON,REMOVE,vam-eval-ego-001,100000",
            sock.messages,
        )
        self.assertEqual(sock.messages[-1], "SUMO_STEP_END,100000")

    def test_vehicle_upsert_keeps_legacy_datagram_shape(self):
        bridge = self.require_bridge()
        sock = FakeSocket()

        bridge.traci.person.getIDList = staticmethod(lambda: [])

        bridge.publish_current_step(sock, set(), set())

        vehicle_message = sock.messages[0]
        fields = vehicle_message.split(",")

        self.assertEqual(len(fields), 7)
        self.assertEqual(fields[0], "veh0")
        self.assertEqual(fields[-1], "100000")

    def test_all_datagrams_target_existing_ns3_udp_endpoint(self):
        bridge = self.require_bridge()
        sock = FakeSocket()

        bridge.publish_current_step(sock, set(), set())

        self.assertTrue(sock.destinations)
        self.assertTrue(
            all(
                destination == ("127.0.0.1", 7400)
                for destination in sock.destinations
            )
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
