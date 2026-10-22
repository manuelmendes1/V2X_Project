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


class TestCpmVehiclePerceptionCodecWiring(unittest.TestCase):
    def test_controller_builds_and_encodes_without_transmitting(self):
        root = repository_root(Path(__file__))
        source_path = (
            root
            / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        )
        source = source_path.read_text()

        start = source.find("SimulationController::RunCpmPerception(std::uint64_t referenceTimeUs)")
        self.assertGreaterEqual(start, 0, "RunCpmPerception não encontrado")

        end = source.find("SimulationController::PollLoop()", start)
        self.assertGreater(end, start, "Fim de RunCpmPerception não encontrado")
        method = source[start:end]

        self.assertIn(
            '#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"',
            source,
            "Header do encoder não incluído no controlador",
        )
        self.assertIn(
            "CpmUperEncoder::Encode(input)",
            method,
            "O resultado de Build ainda não é codificado",
        )
        self.assertIn(
            "[CPM_CODEC] ENCODE_REJECTED",
            method,
            "Falta registo de falhas de codificação",
        )
        self.assertIn(
            "[CPM_CODEC] ENCODE_ACCEPTED",
            method,
            "Falta registo de codificação aceite",
        )
        self.assertIn("encoded.bytes.size()", method)
        self.assertIn("encoded.encodedBits", method)
        self.assertIn("mode=NO_TX", method)
        self.assertNotIn("BtpTxRequest", method)
        self.assertNotIn("sendto(", method)

        print("CPM_CONTROLLER_CODEC_WIRING=PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
