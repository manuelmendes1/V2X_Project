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


class TestCpmPerceptionTimestampGate(unittest.TestCase):
    def test_each_reference_timestamp_is_encoded_at_most_once(self):
        root = repository_root(Path(__file__))
        relative = Path(
            "scratch/vanetza_integration/context/"
            "simulation_controller"
        )

        header = (root / relative / "simulation_controller.h").read_text()
        source = (root / relative / "simulation_controller.cc").read_text()

        start = source.find("SimulationController::RunCpmPerception(std::uint64_t referenceTimeUs)")
        self.assertGreaterEqual(start, 0, "RunCpmPerception não encontrado")

        end = source.find("SimulationController::PollLoop()", start)
        self.assertGreater(end, start, "PollLoop não encontrado")
        method = source[start:end]

        self.assertIn(
            "m_cpmLastEncodedReferenceTimeUs",
            header,
            "Falta estado para controlar o último timestamp CPM codificado",
        )

        guard = method.find(
            "if (m_cpmLastEncodedReferenceTimeUs"
        )
        self.assertGreaterEqual(
            guard, 0,
            "Falta a guarda contra timestamps CPM repetidos",
        )

        comparison = method.find(
            "referenceTimeUs <= *m_cpmLastEncodedReferenceTimeUs",
            guard,
        )
        build = method.find("m_cpmPerceptionSource->Build(")

        self.assertGreaterEqual(
            comparison, 0,
            "A guarda deve rejeitar timestamps iguais ou anteriores",
        )
        self.assertGreater(
            build, guard,
            "A guarda tem de ocorrer antes da construção do CPM",
        )
        self.assertLess(
            comparison, build,
            "A comparação temporal tem de preceder Build",
        )

        encode = method.find("CpmUperEncoder::Encode(input)")
        failure = method.find("if (!encoded.success)", encode)
        failure_return = method.find("return;", failure)
        update = method.find(
            "m_cpmLastEncodedReferenceTimeUs = referenceTimeUs"
        )

        self.assertGreaterEqual(encode, 0, "Chamada ao encoder não encontrada")
        self.assertGreaterEqual(failure, 0, "Tratamento de erro não encontrado")
        self.assertGreaterEqual(
            failure_return, 0,
            "O ramo de falha deve terminar sem marcar o timestamp como aceite",
        )
        self.assertGreater(
            update, failure_return,
            "O timestamp só pode ser marcado depois de a codificação ter sucesso",
        )

        configure = source.find("SimulationController::ConfigureCpmPerception")
        self.assertGreaterEqual(configure, 0, "Configuração CPM não encontrada")
        self.assertIn(
            "m_cpmLastEncodedReferenceTimeUs.reset()",
            source[configure:],
            "A configuração CPM deve reinicializar a guarda temporal",
        )

        print("CPM_PERCEPTION_TIMESTAMP_GATE=PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
