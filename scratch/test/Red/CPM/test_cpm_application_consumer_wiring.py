from pathlib import Path
import re
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestCpmApplicationConsumerWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = repository_root(Path(__file__))
        cls.header = (
            root / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.h"
        ).read_text()
        cls.source = (
            root / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        ).read_text()

    def test_controller_declares_consumer(self):
        self.assertRegex(
            self.header,
            r"static\s+void\s+DrainCpmReceptionQueues\s*\(",
            "CPM_APPLICATION_DRAIN_NOT_DECLARED",
        )

    def test_poll_loop_drains_after_simulation_step(self):
        match = re.search(
            r"void\s+SimulationController::PollLoop\s*\(\s*\)"
            r"\s*\{(?P<body>.*?)^\}",
            self.source,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(match, "POLL_LOOP_NOT_FOUND")
        body_without_comments = re.sub(
            r"//[^\n]*",
            "",
            match.group("body"),
        )
        self.assertRegex(
            body_without_comments,
            r"SimulationStep\s*\(\s*\)\s*;\s*"
            r"DrainCpmReceptionQueues\s*\(\s*\)\s*;",
            "CPM_APPLICATION_DRAIN_NOT_CALLED_AFTER_SIMULATION_STEP",
        )

    def test_consumer_pops_validated_messages_and_logs_semantics(self):
        match = re.search(
            r"void\s+SimulationController::DrainCpmReceptionQueues"
            r"\s*\(\s*\)\s*\{(?P<body>.*?)^\}",
            self.source,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(match, "CPM_APPLICATION_DRAIN_NOT_IMPLEMENTED")
        body = match.group("body")

        for expected, message in (
            ("VehicleRegistry::Instance()", "CPM_VEHICLE_REGISTRY_NOT_USED"),
            ("GetAllVehicles()", "CPM_RECEIVER_STATES_NOT_ENUMERATED"),
            ("TryReceiveCpm(message)", "CPM_RUNTIME_QUEUE_NOT_CONSUMED"),
            ("CPM_APP_RX", "CPM_APPLICATION_DELIVERY_NOT_OBSERVABLE"),
            ("message.station.stationId", "CPM_SENDER_ID_NOT_LOGGED"),
            ("message.knownObjectCount", "CPM_KNOWN_COUNT_NOT_LOGGED"),
            ("message.perceivedObjects.size()", "CPM_INCLUDED_COUNT_NOT_LOGGED"),
            ("object.objectId", "CPM_OBJECT_ID_NOT_LOGGED"),
            ("object.position.xEast.value", "CPM_OBJECT_X_NOT_LOGGED"),
            ("object.position.yNorth.value", "CPM_OBJECT_Y_NOT_LOGGED"),
        ):
            with self.subTest(contract=message):
                self.assertIn(expected, body, message)


if __name__ == "__main__":
    unittest.main(verbosity=2)
