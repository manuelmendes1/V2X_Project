import unittest
from pathlib import Path


def repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestCpmRsuConfigMode(unittest.TestCase):
    def test_enabled_configuration_does_not_report_build_only(self):
        root = repository_root(Path(__file__))
        source_path = (
            root
            / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        )
        source = source_path.read_text()

        start = source.find("SimulationController::ConfigureCpmPerception(")
        self.assertGreaterEqual(start, 0, "ConfigureCpmPerception não encontrado")

        end = source.find("SimulationController::SetCamEnabled(", start)
        self.assertGreater(end, start, "Fim de ConfigureCpmPerception não encontrado")
        method = source[start:end]

        self.assertIn(
            'mode=TX_ENABLED',
            method,
            "A configuração CPM ativa deve indicar que a transmissão está habilitada",
        )
        self.assertNotIn(
            'mode=BUILD_ONLY',
            method,
            "O log da configuração ainda descreve o modo antigo sem transmissão",
        )

        print("CPM_RSU_CONFIG_MODE=PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
