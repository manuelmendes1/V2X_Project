import unittest
from pathlib import Path


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestCpmRsuTransmitControllerWiring(unittest.TestCase):
    def test_controller_initializes_rsu_and_sends_cpm(self):
        root = repository_root(Path(__file__))

        header = (
            root / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.h"
        ).read_text()
        controller = (
            root / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        ).read_text()
        main = (root / "scratch/v2x/v2x_sumo.cc").read_text()
        cmake = (root / "scratch/CMakeLists.txt").read_text()

        start = controller.find(
            "SimulationController::RunCpmPerception("
            "std::uint64_t referenceTimeUs)"
        )
        end = controller.find("SimulationController::PollLoop()", start)
        self.assertGreaterEqual(start, 0, "RunCpmPerception não encontrado")
        self.assertGreater(end, start, "Fim de RunCpmPerception não encontrado")
        run_method = controller[start:end]

        checks = {
            "RSU_ENDPOINT_MEMBER":
                "std::unique_ptr<CpmEmitterEndpoint>" in header,
            "RSU_RUNTIME_MEMBER":
                "std::unique_ptr<CpmRsuRuntime>" in header,
            "PHYSICAL_RSU_XY_CONFIGURATION":
                "double ns3X" in header and "double ns3Y" in header
                and "m_cpmNs3X" in header and "m_cpmNs3Y" in header,
            "CLI_RSU_PHYSICAL_COORDINATES":
                '"cpmNs3X"' in main and '"cpmNs3Y"' in main,
            "CLI_COORDINATES_PASSED_TO_CONTROLLER":
                "cpmNs3X" in main and "cpmNs3Y" in main
                and "ConfigureCpmPerception(" in main,
            "DEDICATED_RSU_RUNTIME_INCLUDES":
                "cpm/emitter/cpm_emitter_endpoint.h" in controller
                and "cpm/emitter/cpm_rsu_runtime.h" in controller,
            "RSU_RUNTIME_LAZY_INITIALIZATION":
                "EnsureCpmRuntime()" in run_method
                and "VehicleNetworkBuilder::InitializeCpmEmitter(" in controller
                and ("runtime->Initialize()" in controller or "m_cpmRuntime->Initialize()" in controller),
            "CPM_SERVICE_ACTUALLY_SENT":
                "m_cpmRuntime->Send(" in run_method,
            "PACKET_CONTEXT_HAS_GLOBAL_PACKET_ID":
                "PacketIdManager::Instance().NextPacketId()" in run_method,
            "PACKET_CONTEXT_HAS_CPM_SEQUENCE":
                "PayloadType::CPM" in run_method
                and "NextSequenceId(" in run_method,
            "RSU_IDENTITY_USED":
                "m_cpmEmitter->StationId()" in run_method
                and "m_cpmEmitter->GetNode()->GetId()" in run_method,
            "RSU_SHUTDOWN_DECLARED":
                "ShutdownCpmEmitter()" in header
                and "SimulationController::ShutdownCpmEmitter()" in controller,
            "RUNTIME_SOURCES_REGISTERED":
                "cpm/emitter/cpm_emitter_endpoint.cc" in cmake
                and "cpm/emitter/cpm_rsu_runtime.cc" in cmake,
        }

        for name, passed in checks.items():
            print(f'{"PASS" if passed else "FAIL"} {name}')

        missing = [name for name, passed in checks.items() if not passed]
        self.assertEqual(
            missing,
            [],
            "CPM_RSU_CONTROLLER_WIRING_MISSING=" + ",".join(missing),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
