"""Gate estrutural para a chamada ativa do serviço de transmissão SPATEM."""

import re
import unittest
from pathlib import Path


def find_repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / ".git").exists()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate

    raise RuntimeError("Não foi possível localizar a raiz do ns-3.")


class TestSpatemTxDispatch(unittest.TestCase):
    def test_spatem_service_has_an_active_call_site(self):
        root = find_repository_root(Path(__file__))
        integration = root / "scratch" / "vanetza_integration"
        source_file = (
            integration / "spatem" / "service" / "spatem_service.cc"
        )

        if not source_file.is_file():
            self.fail(f"Serviço SPATEM não encontrado: {source_file}")

        pattern = re.compile(r"\bSpatemService\s*::\s*Send\s*\(")
        call_sites = []

        search_roots = [
            root / "scratch" / "v2x",
            integration,
        ]

        excluded = {
            source_file.resolve(),
            Path(__file__).resolve(),
        }

        visited = set()

        for search_root in search_roots:
            for path in search_root.rglob("*"):
                if (
                    not path.is_file()
                    or path.suffix not in {".cc", ".h"}
                ):
                    continue

                resolved = path.resolve()
                if resolved in excluded or resolved in visited:
                    continue

                visited.add(resolved)

                text = path.read_text(
                    encoding="utf-8",
                    errors="replace",
                )

                if pattern.search(text):
                    call_sites.append(
                        str(path.relative_to(root))
                    )

        self.assertTrue(
            call_sites,
            "RED: SpatemService::Send() não tem um chamador ativo "
            "identificado fora da sua própria implementação. "
            "A existência do serviço e do endpoint RSU não demonstra "
            "que os dados dos semáforos são codificados e transmitidos.",
        )

        self.assertGreater(
            len(call_sites),
            0,
            "Era esperado pelo menos um chamador de SpatemService::Send().",
        )

        controller_file = (
            integration / "context" / "simulation_controller"
            / "simulation_controller.cc"
        )
        controller_source = controller_file.read_text(
            encoding="utf-8", errors="replace"
        )
        self.assertRegex(
            controller_source,
            re.compile(r"\bm_spatemRuntime\s*->\s*Send\s*\("),
            "RED: o controlador SUMO ainda não encaminha as atualizações "
            "do semáforo para SpatemRsuRuntime::Send().",
        )


if __name__ == "__main__":
    unittest.main()
