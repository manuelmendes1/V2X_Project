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
    def test_controller_delegates_encoding_and_transmission_to_runtime(self):
        root = repository_root(Path(__file__))
        controller_path = (
            root
            / "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        )
        runtime_path = (
            root
            / "scratch/vanetza_integration/cpm/emitter/cpm_rsu_runtime.cc"
        )
        service_path = (
            root / "scratch/vanetza_integration/cpm/service/cpm_service.cc"
        )

        controller = controller_path.read_text()
        runtime = runtime_path.read_text()
        service = service_path.read_text()

        start = controller.find(
            "SimulationController::RunCpmPerception(std::uint64_t referenceTimeUs)"
        )
        self.assertGreaterEqual(start, 0, "RunCpmPerception não encontrado")

        end = controller.find("SimulationController::PollLoop()", start)
        self.assertGreater(end, start, "Fim de RunCpmPerception não encontrado")

        method = controller[start:end]
        method_normalized = " ".join(method.split())
        runtime_normalized = " ".join(runtime.split())
        service_normalized = " ".join(service.split())

        build = method_normalized.find("m_cpmPerceptionSource->Build(")
        ensure_runtime = method_normalized.find("EnsureCpmRuntime()")
        send = method_normalized.find(
            "m_cpmRuntime->Send(context, input, cpmTrafficClass)"
        )

        self.assertGreaterEqual(build, 0, "Falta a construção do modelo CPM")
        self.assertGreater(
            ensure_runtime, build, "O runtime deve ser assegurado após Build"
        )
        self.assertGreater(
            send,
            ensure_runtime,
            "O controlador deve delegar a transmissão ao runtime",
        )

        self.assertNotIn(
            "CpmUperEncoder::Encode(input)",
            method,
            "A codificação deve ficar na camada CpmService",
        )
        self.assertNotIn(
            "vanetza_integration/cpm/codec/cpm_uper_encoder.h",
            controller,
            "O controlador não deve depender diretamente do encoder",
        )
        self.assertNotIn("BtpTxRequest", method)
        self.assertNotIn("sendto(", method)

        self.assertRegex(
            runtime_normalized,
            r"return\s+CpmService::Send\s*\(\s*context,\s*input,\s*gateway,\s*trafficClass\s*\)\s*;",
            "CpmRsuRuntime deve delegar para CpmService",
        )
        self.assertIn(
            "CpmUperEncoder::Encode(input)",
            service_normalized,
            "CpmService deve codificar o input CPM",
        )
        self.assertIn(
            "encoded.success",
            service_normalized,
            "CpmService deve validar o resultado da codificação",
        )
        self.assertIn(
            "encoded.bytes.empty()",
            service_normalized,
            "CpmService deve validar o payload codificado",
        )

        print("CPM_RUNTIME_CODEC_WIRING=PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
