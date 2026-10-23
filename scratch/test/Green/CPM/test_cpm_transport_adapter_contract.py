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


class TestCpmTransportAdapterContract(unittest.TestCase):
    def test_optional_transport_limits_reach_vanetza_request(self):
        root = repository_root(Path(__file__))
        source_path = (
            root / "scratch/vanetza_integration/wrappers/vanetza/btp/"
            "vanetza_btp_adapter.cc"
        )
        source = source_path.read_text()

        expected_contract = {
            "CPM_LIFETIME_OVERRIDE_READ":
                "txRequest.maximumLifetimeMs.has_value()",
            "CPM_LIFETIME_MIB_BOUND":
                "itsGnMaxPacketLifetime.decode()",
            "CPM_LIFETIME_FORWARDED":
                "request.gn.maximum_lifetime",
            "CPM_HOP_LIMIT_OVERRIDE_READ":
                "txRequest.maximumHopLimit.has_value()",
            "CPM_HOP_LIMIT_FORWARDED":
                "request.gn.maximum_hop_limit",
        }

        for marker, snippet in expected_contract.items():
            with self.subTest(contract=marker):
                self.assertIn(
                    snippet,
                    source,
                    f"Contrato de transporte ausente: {marker}",
                )

        print("CPM_TRANSPORT_ADAPTER_SOURCE_CONTRACT=PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
