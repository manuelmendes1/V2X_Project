from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[5]


def source(relative_path):
    path = ROOT / relative_path
    if not path.is_file():
        raise FileNotFoundError(path)
    return path.read_text(encoding="utf-8")


class TestSpatemRsuPhysicalPosition(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main = source("scratch/v2x/v2x_sumo.cc")
        cls.controller_h = source(
            "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.h"
        )
        cls.controller_cc = source(
            "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        )
        cls.endpoint_h = source(
            "scratch/vanetza_integration/spatem/emitter/"
            "spatem_emitter_endpoint.h"
        )
        cls.endpoint_cc = source(
            "scratch/vanetza_integration/spatem/emitter/"
            "spatem_emitter_endpoint.cc"
        )
        cls.builder_h = source(
            "scratch/vanetza_integration/context/its_station/network/"
            "its_station_network_builder.h"
        )
        cls.builder_cc = source(
            "scratch/vanetza_integration/context/its_station/network/"
            "its_station_network_builder.cc"
        )

    def test_cli_exposes_independent_physical_coordinates(self):
        self.assertRegex(
            self.main,
            r'cmd\.AddValue\(\s*"spatemNs3X"',
        )
        self.assertRegex(
            self.main,
            r'cmd\.AddValue\(\s*"spatemNs3Y"',
        )

    def test_coordinates_reach_the_endpoint_configuration(self):
        for name in ("spatemNs3X", "spatemNs3Y"):
            with self.subTest(parameter=name):
                self.assertIn(name, self.controller_h)
                self.assertIn(name, self.controller_cc)

        self.assertRegex(
            self.endpoint_h,
            r'double\s+ns3X\b',
        )
        self.assertRegex(
            self.endpoint_h,
            r'double\s+ns3Y\b',
        )
        self.assertIn("config.ns3X", self.controller_cc)
        self.assertIn("config.ns3Y", self.controller_cc)

    def test_endpoint_passes_physical_position_to_network_builder(self):
        self.assertRegex(
            self.endpoint_cc,
            r'networkConfig\.initialPosition\s*=\s*'
            r'Vector\s*\(\s*m_config\.ns3X\s*,'
            r'\s*m_config\.ns3Y\s*,\s*0(?:\.0)?\s*\)',
        )
        self.assertIn("struct ItsStationNetworkConfig", self.builder_h)
        self.assertIn("Vector initialPosition;", self.builder_h)
        self.assertIn(
            "const ItsStationNetworkConfig& config",
            self.builder_h,
        )
        self.assertIn(
            "std::function<void(Ptr<Node>)> mobilityInstaller;",
            self.builder_h,
        )

    def test_position_is_applied_before_wifi_installation(self):
        mobility_installer = self.builder_cc.find(
            "config.mobilityInstaller(node)"
        )
        mobility_model_check = self.builder_cc.find(
            "node->GetObject<MobilityModel>()"
        )
        position = self.builder_cc.find(
            "mobility->SetPosition(config.initialPosition)"
        )
        wifi_install = self.builder_cc.find("wifi.Install(")

        self.assertGreaterEqual(
            mobility_installer,
            0,
            "O builder não instala o modelo de mobilidade.",
        )
        self.assertGreaterEqual(
            mobility_model_check,
            0,
            "O builder não confirma a existência de MobilityModel.",
        )
        self.assertGreaterEqual(
            position,
            0,
            "A posição inicial não é aplicada ao MobilityModel.",
        )
        self.assertGreaterEqual(
            wifi_install,
            0,
            "Não foi encontrada a instalação Wi-Fi.",
        )

        self.assertLess(
            mobility_installer,
            mobility_model_check,
            "O modelo de mobilidade deve ser instalado antes da sua validação.",
        )
        self.assertLess(
            mobility_model_check,
            position,
            "A posição deve ser aplicada depois da instalação do modelo.",
        )
        self.assertLess(
            position,
            wifi_install,
            "A posição física deve ser aplicada antes da instalação Wi-Fi.",
        )

    def test_physical_coordinates_must_be_finite(self):
        self.assertIn("std::isfinite(config.ns3X)", self.endpoint_cc)
        self.assertIn("std::isfinite(config.ns3Y)", self.endpoint_cc)


if __name__ == "__main__":
    unittest.main(verbosity=2)
