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
        base = root / "scratch/vanetza_integration/context/simulation_controller"

        header = (base / "simulation_controller.h").read_text()
        source = (base / "simulation_controller.cc").read_text()
        service = (
            root / "scratch/vanetza_integration/cpm/service/cpm_service.cc"
        ).read_text()

        start = source.find(
            "SimulationController::RunCpmPerception(std::uint64_t referenceTimeUs)"
        )
        self.assertGreaterEqual(start, 0, "RunCpmPerception não encontrado")

        end = source.find("SimulationController::PollLoop()", start)
        self.assertGreater(end, start, "PollLoop não encontrado")

        method = source[start:end]
        normalized = " ".join(method.split())
        service_normalized = " ".join(service.split())

        self.assertIn(
            "m_cpmLastEncodedReferenceTimeUs",
            header,
            "Falta estado para controlar o último timestamp codificado",
        )

        guard = normalized.find(
            "if (m_cpmLastEncodedReferenceTimeUs.has_value() && "
            "referenceTimeUs <= *m_cpmLastEncodedReferenceTimeUs)"
        )
        build = normalized.find("m_cpmPerceptionSource->Build(")
        send = normalized.find(
            "m_cpmRuntime->Send(context, input, cpmTrafficClass)"
        )
        payload_gate = normalized.find(
            "if (context->payload.type == PayloadType::CPM && "
            "!context->payload.bytes.empty())"
        )
        update = normalized.find(
            "m_cpmLastEncodedReferenceTimeUs = referenceTimeUs"
        )

        self.assertGreaterEqual(guard, 0, "Guarda temporal não encontrada")
        self.assertGreater(build, guard, "Build deve ocorrer depois da guarda")
        self.assertGreater(send, build, "O runtime deve ser chamado após Build")
        self.assertGreater(
            payload_gate,
            send,
            "A marcação temporal deve verificar o contexto depois do runtime",
        )
        self.assertGreater(
            update,
            payload_gate,
            "O timestamp só pode ser atualizado sob a guarda dos bytes CPM",
        )

        self.assertIn(
            "CpmUperEncoder::Encode(input)",
            service_normalized,
            "CpmService deve continuar a efetuar a codificação UPER",
        )
        self.assertIn(
            "encoded.bytes.empty()",
            service_normalized,
            "CpmService deve rejeitar uma codificação sem bytes",
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
