import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


class TestCpmSumoStepBoundary(unittest.TestCase):
    def test_bridge_marks_end_of_each_sumo_step(self):
        source = (
            ROOT / "sumo/current/integration/v2x_bridge_16.py"
        ).read_text()

        loop_start = source.find("while should_continue_simulation_us(")
        self.assertGreaterEqual(loop_start, 0, "Loop SUMO não encontrado")

        vehicle = source.find("publish_vehicle_updates(", loop_start)

        vehicle_updates_start = source.find("def publish_vehicle_updates(")
        self.assertGreaterEqual(
            vehicle_updates_start,
            0,
            "Helper publish_vehicle_updates não encontrado",
        )

        vehicle_updates_end = source.find("\ndef ", vehicle_updates_start + 1)
        self.assertGreater(
            vehicle_updates_end,
            vehicle_updates_start,
            "Limites de publish_vehicle_updates não encontrados",
        )

        vehicle_updates = source[vehicle_updates_start:vehicle_updates_end]
        self.assertIn(
            "process_vehicle(",
            vehicle_updates,
            "publish_vehicle_updates deixou de delegar em process_vehicle",
        )
        tls = source.find("publish_traffic_light_updates(", loop_start)
        marker = source.find("SUMO_STEP_END,", loop_start)
        receive = source.find("receive_from_ns3(sock)", loop_start)

        self.assertGreater(vehicle, loop_start)
        self.assertGreater(tls, vehicle)
        self.assertGreater(
            marker, tls,
            "O marcador deve ser emitido depois dos estados e do TLS",
        )
        self.assertGreater(
            receive, marker,
            "O marcador deve preceder a receção de dados do ns-3",
        )
        self.assertIn("sock.sendto(", source[marker - 500:marker + 500])

        print("CPM_SUMO_STEP_BOUNDARY_BRIDGE=PASS")

    def test_adapter_dispatches_step_completion(self):
        header = (
            ROOT / "scratch/vanetza_integration/sumo_adapter/"
            "sumo_adapter.h"
        ).read_text()
        source = (
            ROOT / "scratch/vanetza_integration/sumo_adapter/"
            "sumo_adapter.cc"
        ).read_text()

        self.assertIn("StepCompleteCallback", header)
        self.assertIn("SetStepCompleteCallback", header)
        self.assertIn("SUMO_STEP_END,", source)
        self.assertIn("m_stepCompleteCallback", source)

        marker = source.find("SUMO_STEP_END,")
        parser = source.find("SumoParser::Parse", marker)
        self.assertGreater(
            parser, marker,
            "O marcador deve ser tratado antes do parser de veículos",
        )

        print("CPM_SUMO_STEP_BOUNDARY_ADAPTER=PASS")

    def test_controller_runs_perception_at_step_boundary(self):
        source = (
            ROOT / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        ).read_text()
        header = (
            ROOT / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.h"
        ).read_text()

        self.assertIn("SumoAdapter::SetStepCompleteCallback", source)
        self.assertIn("HandleSumoStepComplete", source)
        self.assertIn("RunCpmPerception(referenceTimeUs)", source)
        self.assertIn(
            "RunCpmPerception(std::uint64_t referenceTimeUs)",
            source,
        )
        self.assertIn("HandleSumoStepComplete", header)

        start = source.find("SimulationController::PollLoop()")
        end = source.find("Independent CAM scheduler", start)
        self.assertGreater(end, start)
        poll_loop = source[start:end]

        self.assertIn("SumoAdapter::Poll();", poll_loop)
        self.assertNotIn("RunCpmPerception();", poll_loop)

        print("CPM_SUMO_STEP_BOUNDARY_CONTROLLER=PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
