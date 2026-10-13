"""Gate estrutural para resolução da identidade de uma RSU por IPv4.

Este teste verifica o código-fonte do caminho RX ativo.
Não constitui, por si só, prova de receção funcional de pacotes RSU.
"""

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


class TestRsuIpv4SenderResolution(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        repo_root = find_repository_root(Path(__file__))
        cls.runtime_file = (
            repo_root
            / "scratch"
            / "vanetza_integration"
            / "vanetza_runtime"
            / "vanetza_runtime.cc"
        )

        if not cls.runtime_file.is_file():
            raise FileNotFoundError(
                f"Ficheiro de runtime não encontrado: {cls.runtime_file}"
            )

        cls.source = cls.runtime_file.read_text(encoding="utf-8")

    def test_ipv4_sender_resolution_is_not_vehicle_only(self):
        """O caminho RX não deve exigir VehicleState para resolver qualquer emissor."""

        vehicle_only_lookup = re.compile(
            r"VehicleState\s*\*\s*sender\s*=\s*"
            r"VehicleRegistry::Instance\(\)\s*"
            r"\.GetByIpv4\(\s*sourceIp\s*\)"
        )

        self.assertIsNone(
            vehicle_only_lookup.search(self.source),
            msg=(
                "RED: o caminho RX continua a resolver sourceIp "
                "diretamente para VehicleState através de "
                "VehicleRegistry::GetByIpv4(). "
                "É necessária uma resolução de identidade que também "
                "represente uma RSU sem lhe atribuir uma identidade SUMO. "
                f"Ficheiro: {self.runtime_file}"
            ),
        )

        # A RSU tem de ser ligada ao cenário ativo, não apenas definida.
        integration_root = self.runtime_file.parents[1]
        endpoint_source = (
            integration_root
            / "spatem"
            / "emitter"
            / "spatem_emitter_endpoint.cc"
        )
        endpoint_header = (
            integration_root
            / "spatem"
            / "emitter"
            / "spatem_emitter_endpoint.h"
        )

        call_sites = []
        for candidate in integration_root.rglob("*"):
            if not candidate.is_file():
                continue
            if candidate.suffix not in (".cc", ".h"):
                continue
            if candidate in (endpoint_source, endpoint_header):
                continue

            text = candidate.read_text(
                encoding="utf-8", errors="replace"
            )

            # A definição do helper não é um chamador ativo.
            builder_impl = (
                integration_root
                / "context"
                / "vehicle_network"
                / "vehicle_network_builder.cc"
            ).resolve()

            if candidate.resolve() == builder_impl:
                continue

            if "VehicleNetworkBuilder::InitializeSpatemEmitter(" in text:
                call_sites.append(
                    str(candidate.relative_to(integration_root))
                )

        self.assertTrue(
            call_sites,
            "RED: não existe uma chamada ativa a "
            "VehicleNetworkBuilder::InitializeSpatemEmitter() "
            "fora da implementação do próprio helper.",
        )



if __name__ == "__main__":
    unittest.main()
