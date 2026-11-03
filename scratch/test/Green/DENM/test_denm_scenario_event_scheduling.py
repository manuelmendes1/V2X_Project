import shutil
import subprocess
import tempfile
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


class TestDenmScenarioEventScheduling(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = find_repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="denm-scenario-scheduling-"
        )
        cls.binary = Path(cls.tempdir.name) / "test_denm_scenario_scheduling"

        compiler = shutil.which("g++-11") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Não foi encontrado g++-11 nem g++.")

        integration = cls.root / "scratch/vanetza_integration"

        test_source = (
            cls.root / "scratch/test/Green/DENM/"
            "test_denm_scenario_event_scheduling.cc"
        )

        production_sources = [
            integration / "denm/scenario/sumo_denm_event_parser.cc",
            integration / "denm/scenario/denm_scenario_event_source.cc",
        ]

        missing = [
            str(source)
            for source in [test_source, *production_sources]
            if not source.is_file()
        ]
        if missing:
            raise RuntimeError(
                "Faltam fontes necessárias:\n" + "\n".join(missing)
            )

        command = [
            compiler,
            "-std=c++23",
            "-O0",
            "-g",
            f"-I{cls.root / 'build/include'}",
            f"-I{cls.root / 'scratch'}",
            str(test_source),
            *(str(source) for source in production_sources),
            str(cls.root / "build/lib/libns3.47-core-default.so"),
            f"-Wl,-rpath,{cls.root / 'build/lib'}",
            "-o",
            str(cls.binary),
        ]

        compiled = subprocess.run(
            command,
            cwd=cls.root,
            capture_output=True,
            text=True,
            check=False,
        )

        if compiled.returncode != 0:
            raise RuntimeError(
                "Compilação do teste DENM falhou.\n"
                f"Comando: {' '.join(command)}\n"
                f"STDOUT:\n{compiled.stdout}\n"
                f"STDERR:\n{compiled.stderr}"
            )

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_parser_and_scenario_scheduling_contract(self):
        result = subprocess.run(
            [str(self.binary)],
            cwd=self.root,
            capture_output=True,
            text=True,
            check=False,
        )

        output = result.stdout + result.stderr
        print(output, end="", flush=True)

        self.assertEqual(result.returncode, 0, msg=output)
        self.assertIn("DENM_SCENARIO_SCHEDULING_FAILURES=0", output)

    def test_sumo_adapter_dispatches_both_event_prefixes(self):
        adapter = (
            self.root
            / "scratch/vanetza_integration/sumo_adapter/sumo_adapter.cc"
        ).read_text(encoding="utf-8")

        self.assertIn(
            'message.rfind("DENM_EVENT,", 0) == 0',
            adapter,
        )
        self.assertIn(
            'message.rfind("DENM_SCENARIO_EVENT,", 0) == 0',
            adapter,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
