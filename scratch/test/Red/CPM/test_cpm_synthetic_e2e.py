from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 não encontrada")


class TestCpmSyntheticEndToEnd(unittest.TestCase):
    def test_signed_synthetic_cpm_crosses_ns3_and_is_delivered(self):
        root = repository_root(Path(__file__))
        test_source = (
            root / "scratch/test/Red/CPM/test_cpm_synthetic_e2e.cc"
        )

        main_source = root / "scratch/v2x/v2x_sumo.cc"
        main_executable = (
            root / "build/scratch/ns3.47-v2x_sumo-default"
        )
        main_object = (
            "scratch/CMakeFiles/scratch_v2x_sumo.dir/"
            "v2x/v2x_sumo.cc.o"
        )
        target = "ns3.47-v2x_sumo-default"

        self.assertTrue(test_source.is_file(), "E2E harness em falta")
        self.assertTrue(main_executable.is_file(), "Executável de referência em falta")

        required_pki_files = (
            "cpm-root.cert",
            "cpm-aa.cert",
            "cpm-rsu-ticket-000.cert",
            "cpm-rsu-ticket-000.pem",
        )

        configured_pki = os.environ.get("CPM_SYNTHETIC_E2E_PKI")
        if configured_pki:
            pki_dir = Path(configured_pki).expanduser()
            if not pki_dir.is_absolute():
                pki_dir = root / pki_dir
            pki_dir = pki_dir.resolve()
        else:
            pki_candidates = [
                candidate
                for candidate in (root / "test-results").glob(
                    "cpm_security_pki_*"
                )
                if candidate.is_dir()
                and all(
                    (candidate / name).is_file()
                    for name in required_pki_files
                )
            ]

            if not pki_candidates:
                self.fail(
                    "Fixture PKI CPM não encontrada. Prepare a fixture "
                    "de teste e defina CPM_SYNTHETIC_E2E_PKI com o seu "
                    "caminho absoluto."
                )

            if len(pki_candidates) != 1:
                self.fail(
                    "Existem várias fixtures PKI CPM candidatas. "
                    "Defina CPM_SYNTHETIC_E2E_PKI explicitamente: "
                    + ", ".join(str(candidate) for candidate in pki_candidates)
                )

            pki_dir = pki_candidates[0].resolve()

        self.assertTrue(
            pki_dir.is_dir(),
            f"Diretoria PKI CPM não encontrada: {pki_dir}",
        )
        missing_pki_files = [
            name for name in required_pki_files
            if not (pki_dir / name).is_file()
        ]
        self.assertFalse(
            missing_pki_files,
            f"Fixture PKI CPM incompleta em {pki_dir}; faltam: "
            + ", ".join(missing_pki_files),
        )

        command_result = subprocess.run(
            [
                "ninja",
                "-C",
                "cmake-cache",
                "-t",
                "commands",
                target,
            ],
            cwd=root,
            text=True,
            capture_output=True,
            timeout=30,
        )
        self.assertEqual(
            command_result.returncode,
            0,
            "Não foi possível obter a receita Ninja:\n"
            + command_result.stderr,
        )

        commands = command_result.stdout.splitlines()
        compile_commands = [
            line for line in commands
            if str(main_source) in line and re.search(r"\s-c\s", line)
        ]
        link_commands = [
            line for line in commands
            if str(main_executable) in line
            and main_object in line
            and " -o " in line
        ]

        self.assertEqual(
            len(compile_commands), 1,
            "A receita de compilação do main não é única",
        )
        self.assertEqual(
            len(link_commands), 1,
            "A receita de link do executável não é única",
        )

        with tempfile.TemporaryDirectory(
            prefix="cpm-synthetic-e2e-"
        ) as temporary:
            temp = Path(temporary)
            test_object = temp / "test_cpm_synthetic_e2e.cc.o"
            test_binary = temp / "test_cpm_synthetic_e2e"
            runtime_dir = temp / "runtime"
            runtime_dir.mkdir()

            compile_command = compile_commands[0]
            compile_command = compile_command.replace(
                str(main_source), str(test_source)
            )
            compile_command = compile_command.replace(
                main_object + ".d", str(test_object) + ".d"
            )
            compile_command = compile_command.replace(
                main_object, str(test_object)
            )

            compile_result = subprocess.run(
                compile_command,
                shell=True,
                executable="/bin/bash",
                cwd=root / "cmake-cache",
                text=True,
                capture_output=True,
                timeout=180,
            )
            if compile_result.returncode != 0:
                self.fail(
                    "CPM_SYNTHETIC_E2E_COMPILE=FAIL\n"
                    f"STDOUT:\n{compile_result.stdout[-5000:]}\n"
                    f"STDERR:\n{compile_result.stderr[-12000:]}"
                )

            link_command = link_commands[0]
            link_command = link_command.replace(
                str(main_executable), str(test_binary)
            )
            link_command = link_command.replace(
                main_object, str(test_object)
            )

            link_result = subprocess.run(
                link_command,
                shell=True,
                executable="/bin/bash",
                cwd=root / "cmake-cache",
                text=True,
                capture_output=True,
                timeout=180,
            )
            if link_result.returncode != 0:
                self.fail(
                    "CPM_SYNTHETIC_E2E_LINK=FAIL\n"
                    f"STDOUT:\n{link_result.stdout[-5000:]}\n"
                    f"STDERR:\n{link_result.stderr[-12000:]}"
                )

            env = os.environ.copy()
            runtime_paths = [
                str(root / "build/lib"),
                str(Path.home() / "opt/vanetza-cpm-gcc11/lib"),
                str(Path.home() / "opt/geographiclib-gcc11/lib"),
                "/usr/local/lib",
            ]
            existing = env.get("LD_LIBRARY_PATH", "")
            if existing:
                runtime_paths.append(existing)
            env["LD_LIBRARY_PATH"] = ":".join(runtime_paths)

            run = subprocess.run(
                [str(test_binary), str(pki_dir)],
                cwd=runtime_dir,
                env=env,
                text=True,
                capture_output=True,
                timeout=35,
            )

            log_parts = [run.stdout, run.stderr]
            for path in runtime_dir.rglob("*"):
                if not path.is_file() or path.stat().st_size > 20_000_000:
                    continue
                try:
                    log_parts.append(path.read_text(errors="replace"))
                except OSError:
                    pass

            output = "\n".join(log_parts)

            print(f"CPM_SYNTHETIC_E2E_PKI={pki_dir}")
            print(f"CPM_SYNTHETIC_E2E_RUNTIME_RC={run.returncode}")
            relevant = (
                "CPM_SYNTHETIC_E2E",
                "CPM_V3_VERIFY",
                "CPM_RX_DISPATCH",
                "CPM_ENCODE_OK",
                "CPM_BTP_RX_OK",
                "CPM_RX] BTP handler",
            )
            for line in output.splitlines():
                if any(marker in line for marker in relevant):
                    print(line)

            self.assertEqual(
                run.returncode,
                0,
                "O harness E2E falhou. Últimas linhas do log:\n"
                + "\n".join(output.splitlines()[-100:]),
            )
            self.assertIn("CPM_SYNTHETIC_E2E=PASS", output)
            self.assertIn("CPM_SYNTHETIC_E2E_SEMANTICS=PASS", output)
            self.assertRegex(
                output,
                r"\[CPM_V3_VERIFY\].*cryptographic=PASS",
                "Não foi observada verificação criptográfica CPM bem-sucedida",
            )

            tx_match = re.search(
                r"\[CPM_SYNTHETIC_E2E_TX\].*packet_id=(\d+).*"
                r"payload_fnv1a64=([0-9a-f]{16})",
                output,
            )
            self.assertIsNotNone(
                tx_match, "A evidência TX não contém packetId e fingerprint"
            )
            tx_packet_id, fingerprint = tx_match.groups()

            rx_dispatch_lines = [
                line for line in output.splitlines()
                if "CPM_RX_DISPATCH" in line
            ]
            self.assertTrue(
                rx_dispatch_lines,
                "O evento CPM_RX_DISPATCH não foi observado",
            )
            packet_id_pattern = re.compile(
                rf"""packet_id["']?\s*[:=]\s*["']?{re.escape(tx_packet_id)}(?:\D|$)"""
            )
            self.assertTrue(
                any(packet_id_pattern.search(line) for line in rx_dispatch_lines),
                f"packetId TX={tx_packet_id} não foi correlacionado com RX: "
                + "\n".join(rx_dispatch_lines),
            )

            tx_encode_lines = [
                line for line in output.splitlines()
                if "CPM_ENCODE_OK" in line
            ]
            rx_payload_lines = [
                line for line in output.splitlines()
                if "CPM_BTP_RX_OK" in line
            ]

            self.assertTrue(tx_encode_lines, "CPM_ENCODE_OK não foi registado")
            self.assertTrue(rx_payload_lines, "CPM_BTP_RX_OK não foi registado")
            self.assertTrue(
                any(f"payload_fnv1a64={fingerprint}" in line
                    for line in tx_encode_lines),
                "O log TX não contém fingerprint do UPER codificado",
            )
            self.assertTrue(
                any(f"payload_fnv1a64={fingerprint}" in line
                    for line in rx_payload_lines),
                "O fingerprint do payload recebido difere ou ainda não é registado",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
