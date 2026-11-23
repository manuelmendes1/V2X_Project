import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def repository_root(start):
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / ".git").exists()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada")


class TestVehicleCredentialDiscovery(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.tempdir = tempfile.TemporaryDirectory(
            prefix="vehicle-credential-discovery-build-"
        )
        temp = Path(cls.tempdir.name)

        cls.binary = temp / "test_vehicle_credential_discovery"

        main_source = cls.root / "scratch/v2x/v2x_sumo.cc"
        main_executable = (
            cls.root / "build/scratch/ns3.47-v2x_sumo-default"
        )
        main_object = (
            "scratch/CMakeFiles/scratch_v2x_sumo.dir/"
            "v2x/v2x_sumo.cc.o"
        )
        test_source = (
            cls.root / "scratch/test/Red/Context/"
            "test_vehicle_credential_discovery.cc"
        )

        cls.assertFile = staticmethod(
            lambda path, message: (
                None if path.is_file() else
                (_ for _ in ()).throw(RuntimeError(message))
            )
        )

        cls.assertFile(main_executable, "EXECUTABLE_NOT_FOUND")
        cls.assertFile(test_source, "TEST_SOURCE_NOT_FOUND")

        result = subprocess.run(
            ["ninja", "-C", "cmake-cache", "-t", "commands",
             "ns3.47-v2x_sumo-default"],
            cwd=cls.root,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )

        if result.returncode != 0:
            raise RuntimeError("NINJA_COMMANDS_FAILED\n" + result.stderr)

        commands = result.stdout.splitlines()

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

        if len(compile_commands) != 1 or len(link_commands) != 1:
            raise RuntimeError(
                "BUILD_RECIPE_RESOLUTION_FAILED "
                f"compile={len(compile_commands)} "
                f"link={len(link_commands)}"
            )

        test_object = temp / "test_vehicle_credential_discovery.cc.o"

        compile_command = compile_commands[0]
        compile_command = compile_command.replace(
            str(main_source), str(test_source)
        ).replace(
            main_object + ".d", str(test_object) + ".d"
        ).replace(
            main_object, str(test_object)
        )

        compiled = subprocess.run(
            compile_command,
            shell=True,
            executable="/bin/bash",
            cwd=cls.root / "cmake-cache",
            capture_output=True,
            text=True,
            timeout=180,
            check=False,
        )

        if compiled.returncode != 0:
            raise RuntimeError(
                "DISCOVERY_TEST_COMPILE=FAIL\n"
                + compiled.stdout[-3000:]
                + compiled.stderr[-12000:]
            )

        link_command = link_commands[0].replace(
            str(main_executable), str(cls.binary)
        ).replace(
            main_object, str(test_object)
        )

        linked = subprocess.run(
            link_command,
            shell=True,
            executable="/bin/bash",
            cwd=cls.root / "cmake-cache",
            capture_output=True,
            text=True,
            timeout=180,
            check=False,
        )

        if linked.returncode != 0:
            raise RuntimeError(
                "DISCOVERY_TEST_LINK=FAIL\n"
                + linked.stdout[-3000:]
                + linked.stderr[-12000:]
            )

        cls.env = os.environ.copy()
        paths = [
            str(cls.root / "build/lib"),
            str(Path.home() / "opt/vanetza-cpm-gcc11/lib"),
            str(Path.home() / "opt/geographiclib-gcc11/lib"),
            "/usr/local/lib",
        ]
        if cls.env.get("LD_LIBRARY_PATH"):
            paths.append(cls.env["LD_LIBRARY_PATH"])
        cls.env["LD_LIBRARY_PATH"] = ":".join(paths)

    def require_pki_run_directory(self, environment_variable):
        configured = self.env.get(environment_variable, "").strip()

        self.assertTrue(
            configured,
            f"{environment_variable}_NOT_SET: configure this variable "
            "with an existing PKI issuance run directory",
        )

        directory = Path(configured).expanduser()

        self.assertTrue(
            directory.is_dir(),
            f"{environment_variable}_DIRECTORY_NOT_FOUND: {directory}",
        )

        try:
            resolved = directory.resolve(strict=True)
        except OSError as error:
            self.fail(
                f"{environment_variable}_RESOLUTION_FAILED: {error}"
            )

        self.assertTrue(
            resolved.is_dir(),
            f"{environment_variable}_RESOLVED_PATH_NOT_DIRECTORY: "
            f"{resolved}",
        )

        return resolved


    def test_vehicle_pki_cli_and_controller_wiring(self):
        main_source = (self.root / "scratch/v2x/v2x_sumo.cc").read_text()
        controller_header = (
            self.root
            / "scratch/vanetza_integration/context/simulation_controller/"
              "simulation_controller.h"
        ).read_text()
        controller_source = (
            self.root
            / "scratch/vanetza_integration/context/simulation_controller/"
              "simulation_controller.cc"
        ).read_text()

        self.assertIn('"vehiclePkiRunDir"', main_source)
        self.assertIn('"vehicleCredentialMapFile"', main_source)

        self.assertIn(
            "hasVehiclePkiRunDir != hasVehicleCredentialMap",
            main_source,
        )
        self.assertIn(
            "vehiclePkiConfigured && !denmPkiDir.empty()",
            main_source,
        )

        self.assertIn(
            "static bool ConfigureVehiclePki(",
            controller_header,
        )
        self.assertIn(
            "SimulationController::ConfigureVehiclePki(",
            controller_source,
        )
        self.assertIn(
            "return VehicleSecurityInitializer::ConfigureVehiclePki(",
            controller_source,
        )

        parse_position = main_source.index("cmd.Parse(argc, argv);")
        configure_position = main_source.index(
            "SimulationController::ConfigureVehiclePki("
        )
        factory_position = main_source.index("VehicleFactory::Initialize(")
        start_position = main_source.index("SimulationController::Start(")

        self.assertLess(parse_position, configure_position)
        self.assertLess(configure_position, factory_position)
        self.assertLess(factory_position, start_position)

        self.assertIn(
            "SimulationController::SetDenmPkiDirectory(denmPkiDir);",
            main_source,
        )

        validation_start = main_source.index(
            "// The explicit vehicle PKI configuration is an atomic pair."
        )
        validation_end = main_source.index(
            "// Load the optional network projection",
            validation_start,
        )
        validation_block = main_source[validation_start:validation_end]

        self.assertNotIn(r"'\\n'", validation_block)
        self.assertNotIn(r'\\n"', validation_block)
        self.assertIn(r"'\n'", validation_block)
        self.assertIn(r'\n"', validation_block)

        executable = (
            self.root / "build/scratch/ns3.47-v2x_sumo-default"
        )

        runtime_cases = (
            (
                ["--vehiclePkiRunDir=/tmp/v2x-pki-run"],
                "[VEHICLE_PKI] both --vehiclePkiRunDir",
            ),
            (
                ["--vehicleCredentialMapFile=/tmp/vehicle-map.tsv"],
                "[VEHICLE_PKI] both --vehiclePkiRunDir",
            ),
            (
                [
                    "--vehiclePkiRunDir=/tmp/v2x-pki-run",
                    "--vehicleCredentialMapFile=/tmp/vehicle-map.tsv",
                    "--denmPkiDir=/tmp/legacy-denm-pki",
                ],
                "[VEHICLE_PKI] --denmPkiDir cannot be combined",
            ),
        )

        for arguments, expected_message in runtime_cases:
            result = subprocess.run(
                [str(executable), *arguments],
                cwd=self.root,
                env=self.env,
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )

            output = result.stdout + result.stderr

            self.assertEqual(
                result.returncode,
                2,
                msg=f"arguments={arguments}\\n{output}",
            )
            self.assertIn(expected_message, output)


    def test_real_vehicle_pki_credential_activation(self):
        run_dir = self.require_pki_run_directory("V2X_TEST_VEHICLE_PKI_RUN_DIR")

        certificate = (
            run_dir / "issued-tickets/dual-aid/veh0-ticket-000.cert"
        )
        private_key = (
            run_dir / "issued-tickets/dual-aid/veh0-ticket-000.pem"
        )

        self.assertTrue(certificate.is_file(), "REAL_CERTIFICATE_MISSING")
        self.assertTrue(private_key.is_file(), "REAL_PRIVATE_KEY_MISSING")

        self.assertEqual(
            private_key.stat().st_mode & 0o777,
            0o600,
            "REAL_PRIVATE_KEY_PERMISSION_NOT_0600",
        )

        with tempfile.TemporaryDirectory(
            prefix="real-vehicle-pki-map-"
        ) as directory:
            mapping_file = Path(directory) / "vehicle-credentials.tsv"

            mapping_file.write_text(
                chr(9).join((
                    "0",
                    "issued-tickets/dual-aid",
                    "veh0-ticket",
                )) + chr(10),
                encoding="utf-8",
            )

            mapping_bytes = mapping_file.read_bytes()

            self.assertEqual(
                mapping_bytes.count(bytes((9,))),
                2,
                "MAPPING_MUST_CONTAIN_TWO_REAL_TAB_SEPARATORS",
            )
            self.assertTrue(
                mapping_bytes.endswith(bytes((10,))),
                "MAPPING_MUST_END_WITH_NEWLINE",
            )


            result = subprocess.run(
                [
                    str(self.binary),
                    "--real-pki",
                    str(run_dir),
                    str(mapping_file),
                ],
                cwd=self.root,
                env=self.env,
                capture_output=True,
                text=True,
                timeout=60,
                check=False,
            )

            output = result.stdout + result.stderr
            print(output, end="", flush=True)

            self.assertEqual(result.returncode, 0, msg=output)

            for marker in (
                "REAL_PKI_CONFIG=PASS",
                "REAL_PKI_INITIALIZE=PASS",
                "REAL_PKI_ACTIVE_CREDENTIAL=PASS",
                "REAL_PKI_SECURITY_MATERIAL=PASS",
                "REAL_PKI_CREDENTIAL_PATHS=PASS",
                "REAL_PKI_SUMO_IDENTITY=PASS",
                "REAL_PKI_ACTIVATION_CONTRACT=PASS",
            ):
                self.assertIn(marker, output)


    def test_vehicle_factory_rejects_missing_denm_trust_chain(self):
        source_run = self.require_pki_run_directory("V2X_TEST_VEHICLE_PKI_RUN_DIR")

        relative_files = (
            "trust-store/trusted-root.cert",
            "certificate-chain/aa.cert",
            "certificate-chain/root.cert",
            "issued-tickets/dual-aid/veh0-ticket-000.cert",
            "issued-tickets/dual-aid/veh0-ticket-000.pem",
        )

        for relative in relative_files:
            self.assertTrue(
                (source_run / relative).is_file(),
                f"FACTORY_TRUST_FIXTURE_MISSING: {source_run / relative}",
            )

        with tempfile.TemporaryDirectory(
            prefix="vehicle-factory-missing-aa-"
        ) as directory:
            base = Path(directory)
            broken_run = base / "issuance-run"

            for relative in relative_files:
                target = broken_run / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_run / relative, target)

            # Preserve the real ticket/key pair but deliberately remove AA.
            (broken_run / "certificate-chain/aa.cert").unlink()

            private_key = (
                broken_run
                / "issued-tickets/dual-aid/veh0-ticket-000.pem"
            )
            self.assertEqual(
                private_key.stat().st_mode & 0o777,
                0o600,
                "FACTORY_FIXTURE_PRIVATE_KEY_MODE_NOT_0600",
            )

            mapping_file = base / "vehicle-credentials.tsv"
            mapping_file.write_bytes(
                b"0\tissued-tickets/dual-aid\tveh0-ticket\n"
            )

            result = subprocess.run(
                [
                    str(self.binary),
                    "--real-factory",
                    str(broken_run),
                    str(mapping_file),
                ],
                cwd=self.root,
                env=self.env,
                capture_output=True,
                text=True,
                timeout=90,
                check=False,
            )

            output = result.stdout + result.stderr
            print(output, end="", flush=True)

            self.assertNotEqual(
                result.returncode,
                0,
                msg=(
                    "FACTORY_ACCEPTED_VEHICLE_WITH_MISSING_DENM_AA\n"
                    + output
                ),
            )
            self.assertIn(
                "V2X V3 verifier initialization failed",
                output,
            )
            self.assertIn(
                "DENM vehicle PKI certificate must be a regular "
                "non-symlink file",
                output,
            )

            print("VEHICLE_PKI_ADAPTER_TRUST_FAILURE=PASS", flush=True)


    def test_real_vehicle_factory_runtime_integration(self):
        run_dir = self.require_pki_run_directory("V2X_TEST_VEHICLE_PKI_RUN_DIR")

        certificate = (
            run_dir / "issued-tickets/dual-aid/veh0-ticket-000.cert"
        )
        private_key = (
            run_dir / "issued-tickets/dual-aid/veh0-ticket-000.pem"
        )

        self.assertTrue(certificate.is_file(), "REAL_CERTIFICATE_MISSING")
        self.assertTrue(private_key.is_file(), "REAL_PRIVATE_KEY_MISSING")
        self.assertEqual(
            private_key.stat().st_mode & 0o777,
            0o600,
            "REAL_PRIVATE_KEY_PERMISSION_NOT_0600",
        )

        with tempfile.TemporaryDirectory(
            prefix="real-vehicle-factory-map-"
        ) as directory:
            mapping_file = Path(directory) / "vehicle-credentials.tsv"

            mapping_file.write_bytes(
                chr(9).join((
                    "0",
                    "issued-tickets/dual-aid",
                    "veh0-ticket",
                )).encode("utf-8") + bytes((10,))
            )

            mapping_bytes = mapping_file.read_bytes()

            self.assertEqual(
                mapping_bytes.count(bytes((9,))),
                2,
                "FACTORY_MAPPING_MUST_CONTAIN_TWO_REAL_TABS",
            )
            self.assertTrue(
                mapping_bytes.endswith(bytes((10,))),
                "FACTORY_MAPPING_MUST_END_WITH_NEWLINE",
            )

            result = subprocess.run(
                [
                    str(self.binary),
                    "--real-factory",
                    str(run_dir),
                    str(mapping_file),
                ],
                cwd=self.root,
                env=self.env,
                capture_output=True,
                text=True,
                timeout=90,
                check=False,
            )

            output = result.stdout + result.stderr
            print(output, end="", flush=True)

            self.assertEqual(result.returncode, 0, msg=output)

            for marker in (
                "REAL_FACTORY_PKI_CONFIG=PASS",
                "REAL_FACTORY_NODE=PASS",
                "REAL_FACTORY_IDENTITY=PASS",
                "REAL_FACTORY_ACTIVE_CREDENTIAL=PASS",
                "REAL_FACTORY_SECURITY_MATERIAL=PASS",
                "REAL_FACTORY_TRUST_RUN_DIRECTORY=PASS",
                "REAL_FACTORY_NETWORK=PASS",
                "REAL_FACTORY_RUNTIME=PASS",
                "REAL_FACTORY_RUNTIME_IS_INITIALIZED=PASS",
                "REAL_FACTORY_INTEGRATION_CONTRACT=PASS",
            ):
                self.assertIn(marker, output)


    def test_real_vehicle_denm_v3_signature_and_tx_complete(self):
        run_dir = self.require_pki_run_directory("V2X_TEST_VEHICLE_PKI_RUN_DIR")

        certificate = (
            run_dir / "issued-tickets/dual-aid/veh0-ticket-000.cert"
        )
        private_key = (
            run_dir / "issued-tickets/dual-aid/veh0-ticket-000.pem"
        )

        self.assertTrue(certificate.is_file(), "REAL_CERTIFICATE_MISSING")
        self.assertTrue(private_key.is_file(), "REAL_PRIVATE_KEY_MISSING")
        self.assertEqual(
            private_key.stat().st_mode & 0o777,
            0o600,
            "REAL_PRIVATE_KEY_PERMISSION_NOT_0600",
        )

        with tempfile.TemporaryDirectory(
            prefix="real-denm-signature-map-"
        ) as directory:
            mapping_file = Path(directory) / "vehicle-credentials.tsv"

            mapping_file.write_bytes(
                chr(9).join((
                    "0",
                    "issued-tickets/dual-aid",
                    "veh0-ticket",
                )).encode("utf-8") + bytes((10,))
            )

            mapping_bytes = mapping_file.read_bytes()

            self.assertEqual(
                mapping_bytes.count(bytes((9,))),
                2,
                "DENM_MAPPING_MUST_CONTAIN_TWO_REAL_TABS",
            )
            self.assertTrue(
                mapping_bytes.endswith(bytes((10,))),
                "DENM_MAPPING_MUST_END_WITH_NEWLINE",
            )

            result = subprocess.run(
                [
                    str(self.binary),
                    "--real-denm-signature",
                    str(run_dir),
                    str(mapping_file),
                ],
                cwd=self.root,
                env=self.env,
                capture_output=True,
                text=True,
                timeout=120,
                check=False,
            )

            output = result.stdout + result.stderr
            print(output, end="", flush=True)

            self.assertEqual(result.returncode, 0, msg=output)

            for marker in (
                "REAL_DENM_PKI_CONFIG=PASS",
                "REAL_DENM_ITS_TIME=PASS",
                "REAL_DENM_FACTORY_RUNTIME=PASS",
                "REAL_DENM_LIFECYCLE=PASS",
                "REAL_DENM_ACTION_ID=PASS",
                "REAL_DENM_GENERATION=PASS",
                "REAL_DENM_UPER_PAYLOAD=PASS",
                "REAL_DENM_ROUTER_ACCEPTED=PASS",
                "REAL_DENM_SIGNER_CREDENTIAL_ID=PASS",
                "REAL_DENM_TX_COMPLETE=PASS",
                "REAL_DENM_END_TO_END_CONTRACT=PASS",
            ):
                self.assertIn(marker, output)

            self.assertRegex(
                output,
                r"\[DENM_V3_SIGN\]\s+"
                r"protocol_version=\d+\s+aid=37\s+signed=1",
                "REAL_DENM_V3_SIGNED_MESSAGE_NOT_OBSERVED",
            )

    def test_real_vehicle_denm_v3_end_to_end_receive(self):
        run_dir = self.require_pki_run_directory("V2X_TEST_VEHICLE_PKI_DUAL_RUN_DIR")

        self.assertTrue(run_dir.is_dir(), "DUAL_VEHICLE_ISSUANCE_RUN_MISSING")

        for relative in (
            "trust-store/trusted-root.cert",
            "certificate-chain/aa.cert",
            "certificate-chain/root.cert",
        ):
            self.assertTrue(
                (run_dir / relative).is_file(),
                f"DUAL_VEHICLE_TRUST_CHAIN_FILE_MISSING: {relative}",
            )

        for vehicle_id in ("0", "1"):
            certificate = (
                run_dir
                / "issued-tickets/dual-aid"
                / f"veh{vehicle_id}-ticket-000.cert"
            )
            private_key = (
                run_dir
                / "issued-tickets/dual-aid"
                / f"veh{vehicle_id}-ticket-000.pem"
            )

            self.assertTrue(
                certificate.is_file(),
                f"REAL_CERTIFICATE_MISSING: vehicle={vehicle_id}",
            )
            self.assertTrue(
                private_key.is_file(),
                f"REAL_PRIVATE_KEY_MISSING: vehicle={vehicle_id}",
            )
            self.assertEqual(
                private_key.stat().st_mode & 0o777,
                0o600,
                f"REAL_PRIVATE_KEY_PERMISSION_NOT_0600: vehicle={vehicle_id}",
            )

        with tempfile.TemporaryDirectory(
            prefix="real-denm-v3-rx-map-"
        ) as directory:
            mapping_file = Path(directory) / "vehicle-credentials.tsv"

            mapping_bytes = (
                "0\tissued-tickets/dual-aid\tveh0-ticket\n"
                "1\tissued-tickets/dual-aid\tveh1-ticket\n"
            ).encode("utf-8")

            mapping_file.write_bytes(mapping_bytes)

            self.assertEqual(
                mapping_bytes.count(bytes((9,))),
                4,
                "DENM_RX_MAPPING_MUST_CONTAIN_FOUR_REAL_TABS",
            )
            self.assertEqual(
                len(mapping_bytes.splitlines()),
                2,
                "DENM_RX_MAPPING_MUST_CONTAIN_EXACTLY_TWO_ROWS",
            )
            self.assertTrue(
                mapping_bytes.endswith(bytes((10,))),
                "DENM_RX_MAPPING_MUST_END_WITH_NEWLINE",
            )

            result = subprocess.run(
                [
                    str(self.binary),
                    "--real-denm-rx",
                    str(run_dir),
                    str(mapping_file),
                ],
                cwd=self.root,
                env=self.env,
                capture_output=True,
                text=True,
                timeout=120,
                check=False,
            )

            output = result.stdout + result.stderr
            print(output, end="", flush=True)

            self.assertEqual(result.returncode, 0, msg=output)

            for marker in (
                "REAL_DENM_RX_PKI_CONFIG=PASS",
                "REAL_DENM_RX_ITS_TIME=PASS",
                "REAL_DENM_RX_TWO_VEHICLES=PASS",
                "REAL_DENM_RX_CREDENTIALS=PASS",
                "REAL_DENM_RX_MOBILITY=PASS",
                "REAL_DENM_RX_GENERATION=PASS",
                "REAL_DENM_RX_ROUTER_ACCEPTED=PASS",
                "REAL_DENM_RX_SIGNER_CREDENTIAL_ID=PASS",
                "REAL_DENM_RX_CREDENTIALS=PASS",
                "REAL_DENM_RX_TX_COMPLETE=PASS",
            ):
                self.assertIn(marker, output)

            self.assertIn(
                "[DENM_V3_SIGNER_TX] contains_certificate=1 certificate_id=1",
                output,
                "DENM_RX_SIGNER_CERTIFICATE_NOT_INCLUDED",
            )

            self.assertRegex(
                output,
                r"\[DENM_V3_VERIFY\]\s+"
                r"protocol_version=3\s+aid=37\s+"
                r"report=SUCCESS\s+cryptographic=PASS\s+context=BOUND",
                "DENM_RX_V3_CRYPTOGRAPHIC_SUCCESS_NOT_OBSERVED",
            )

            self.assertRegex(
                output,
                r"\[VEH=1\].*"
                r"\[VERIFY_PERFORMED=1\]\[VERIFY_OK=1\].*"
                r"\[VERIFY_AID=37\].*\[EV=VERIFY_OK\]",
                "DENM_RX_RECEIVER_1_VERIFY_OK_NOT_OBSERVED",
            )

            self.assertRegex(
                output,
                r"\[VEH=1\].*\[EV=DENM_REPLAY_OK\]",
                "DENM_RX_RECEIVER_1_REPLAY_ACCEPTANCE_NOT_OBSERVED",
            )

            self.assertRegex(
                output,
                r"\[VEH=1\].*\[EV=DENM_RECEPTION_OK\] "
                r"sender=0 receiver=1\b",
                "DENM_RX_RECEIVER_1_RECEPTION_NOT_OBSERVED",
            )

            delivery_lines = re.findall(
                r"(?m)^\[SIM_STEP=.*\[VEH=1\].*"
                r"\[EV=DENM_DELIVER_OK\] sender=0 .*receiver=1\b.*$",
                output,
            )

            self.assertEqual(
                len(delivery_lines),
                1,
                "DENM_RX_EXPECTED_EXACTLY_ONE_DELIVERY_FROM_0_TO_1",
            )

            print("REAL_DENM_RX_CONTRACT=PASS", flush=True)

    def test_denm_v3_trust_chain_configuration(self):
        new_run = self.require_pki_run_directory("V2X_TEST_VEHICLE_PKI_DUAL_RUN_DIR")
        old_run = self.require_pki_run_directory("V2X_TEST_VEHICLE_PKI_RUN_DIR")

        relative_files = (
            "trust-store/trusted-root.cert",
            "certificate-chain/aa.cert",
            "certificate-chain/root.cert",
        )

        for source in (new_run, old_run):
            for relative in relative_files:
                self.assertTrue(
                    (source / relative).is_file(),
                    f"TRUST_CHAIN_FIXTURE_MISSING: {source / relative}",
                )

        new_root_bytes = (
            new_run / "certificate-chain/root.cert"
        ).read_bytes()
        old_root_bytes = (
            old_run / "certificate-chain/root.cert"
        ).read_bytes()

        self.assertNotEqual(
            new_root_bytes,
            old_root_bytes,
            "MISMATCHED_ROOT_FIXTURE_NOT_DISTINCT",
        )

        with tempfile.TemporaryDirectory(
            prefix="denm-v3-trust-chain-"
        ) as directory:
            base = Path(directory)

            def make_fixture(name):
                destination = base / name

                for relative in relative_files:
                    target = destination / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(
                        new_run / relative,
                        target,
                        follow_symlinks=False,
                    )

                return destination

            valid = make_fixture("valid")

            missing_trust_root = make_fixture("missing-trust-root")
            (missing_trust_root / relative_files[0]).unlink()

            missing_aa = make_fixture("missing-aa")
            (missing_aa / relative_files[1]).unlink()

            missing_chain_root = make_fixture("missing-chain-root")
            (missing_chain_root / relative_files[2]).unlink()

            symlink_root = make_fixture("symlink-root")
            symlink_path = symlink_root / relative_files[2]
            symlink_path.unlink()
            symlink_path.symlink_to(
                new_run / relative_files[2]
            )

            mismatched_root = make_fixture("mismatched-root")
            shutil.copy2(
                old_run / relative_files[2],
                mismatched_root / relative_files[2],
            )

            cases = (
                ("valid-chain", str(valid), "accept"),
                ("missing-trust-root", str(missing_trust_root), "reject"),
                ("missing-aa", str(missing_aa), "reject"),
                ("missing-chain-root", str(missing_chain_root), "reject"),
                ("symlink-chain-root", str(symlink_root), "reject"),
                ("mismatched-root", str(mismatched_root), "reject"),
                ("legacy-mode", "", "accept"),
            )

            for label, run_directory, expected in cases:
                with self.subTest(case=label):
                    result = subprocess.run(
                        [
                            str(self.binary),
                            "--denm-trust-config",
                            run_directory,
                            expected,
                        ],
                        cwd=self.root,
                        env=self.env,
                        capture_output=True,
                        text=True,
                        timeout=30,
                        check=False,
                    )

                    output = result.stdout + result.stderr
                    print(
                        f"\n===== DENM TRUST CASE: {label} =====",
                        flush=True,
                    )
                    print(output, end="", flush=True)

                    self.assertEqual(
                        result.returncode,
                        0,
                        msg=f"{label}\n{output}",
                    )
                    self.assertIn(
                        "DENM_TRUST_CONFIG_RESULT=PASS",
                        output,
                    )


    def test_vehicle_registry_clear_removes_mid_index(self):
        result = subprocess.run(
            [str(self.binary), "--vehicle-registry-clear-contract"],
            cwd=self.root,
            env=self.env,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )

        output = result.stdout + result.stderr
        print(output, end="", flush=True)

        self.assertEqual(result.returncode, 0, msg=output)

        for marker in (
            "VEHICLE_REGISTRY_MID_REGISTERED=PASS",
            "VEHICLE_REGISTRY_MID_CLEARED=PASS",
            "VEHICLE_REGISTRY_CLEAR_CONTRACT=PASS",
        ):
            self.assertIn(marker, output)


    def test_rx_bridge_nested_context_contract(self):
        result = subprocess.run(
            [str(self.binary), "--rx-bridge-contract"],
            cwd=self.root,
            env=self.env,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )

        output = result.stdout + result.stderr
        print(output, end="", flush=True)

        self.assertEqual(result.returncode, 0, msg=output)

        for marker in (
            "RX_BRIDGE_INITIAL_EMPTY=PASS",
            "RX_BRIDGE_OUTER_BOUND=PASS",
            "RX_BRIDGE_INNER_BOUND=PASS",
            "RX_BRIDGE_NESTED_RESTORE=PASS",
            "RX_BRIDGE_EMPTY_AFTER_NESTED=PASS",
            "RX_BRIDGE_EXCEPTION_RESTORE=PASS",
            "RX_BRIDGE_EMPTY_AFTER_EXCEPTION=PASS",
            "RX_BRIDGE_NESTED_CONTEXT_CONTRACT=PASS",
        ):
            self.assertIn(marker, output)


    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def test_sumo_identity_credential_prefix_and_legacy_contract(self):
        with tempfile.TemporaryDirectory(
            prefix="vehicle-credential-discovery-fixture-"
        ) as directory:
            fixture = Path(directory)

            names = (
                "veh0-ticket-000.cert",
                "veh0-ticket-000.pem",
                "veh0-ticket-002.cert",
                "veh0-ticket-002.pem",
                "veh0-ticket-003.cert",
                "veh0-ticket-004.pem",
                "veh0-ticket-extra-005.cert",
                "veh0-ticket-extra-005.pem",
                "0-ticket-001.cert",
                "0-ticket-001.pem",
            )

            for name in names:
                (fixture / name).touch(mode=0o600)

            outside_credentials = fixture / "outside-credentials"
            outside_credentials.mkdir()

            outside_cert = outside_credentials / "outside.cert"
            outside_key = outside_credentials / "outside.pem"

            outside_cert.write_bytes(b"external certificate fixture")
            outside_key.write_bytes(b"external private-key fixture")

            # Both certificate and key are symlinks outside the credential directory.
            (fixture / "veh0-ticket-005.cert").symlink_to(outside_cert)
            (fixture / "veh0-ticket-005.pem").symlink_to(outside_key)

            # Only the certificate is a symlink.
            (fixture / "veh0-ticket-006.cert").symlink_to(outside_cert)
            (fixture / "veh0-ticket-006.pem").touch(mode=0o600)

            # Only the private key is a symlink.
            (fixture / "veh0-ticket-007.cert").touch(mode=0o600)
            (fixture / "veh0-ticket-007.pem").symlink_to(outside_key)

            mapping_dir = fixture / "mapping-fixtures"
            mapping_dir.mkdir()

            run_dir = mapping_dir / "pki-run"
            credential_dir = (
                run_dir / "issued-tickets" / "dual-aid"
            )
            credential_dir.mkdir(parents=True)

            # Two deliberately invalid pairs: discovery finds them, but
            # certificate/private-key binding validation must reject them.
            for sequence in ("000", "001"):
                (credential_dir / f"veh0-ticket-{sequence}.cert").write_bytes(
                    b""
                )
                (credential_dir / f"veh0-ticket-{sequence}.pem").write_bytes(
                    b""
                )

            outside_dir = mapping_dir / "outside"
            outside_dir.mkdir()

            (run_dir / "escape").symlink_to(
                outside_dir,
                target_is_directory=True,
            )

            (mapping_dir / "not-a-directory").write_text(
                "not a directory\\n"
            )

            mapping_fixtures = {
                "valid.tsv": (
                    b"# SUMO ID to credential mapping\r\n"
                    b"\r\n"
                    b"0\tissued-tickets/dual-aid\tveh0-ticket\r\n"
                    b"vehicle_1\tissued-tickets/dual-aid\tveh1-ticket\r\n"
                ),
                "duplicate.tsv": (
                    b"0\tissued-tickets/dual-aid\tveh0-ticket\n"
                    b"0\tissued-tickets/dual-aid\tveh0-ticket-duplicate\n"
                ),
                "missing-column.tsv": (
                    b"0\tissued-tickets/dual-aid\n"
                ),
                "extra-column.tsv": (
                    b"0\tissued-tickets/dual-aid\tveh0-ticket\textra\n"
                ),
                "empty-field.tsv": (
                    b"0\tissued-tickets/dual-aid\t\n"
                ),
                "absolute-directory.tsv": (
                    b"0\t/etc/v2x-pki\tveh0-ticket\n"
                ),
                "traversal-directory.tsv": (
                    b"0\tissued-tickets/../../outside\tveh0-ticket\n"
                ),
                "unsafe-prefix.tsv": (
                    b"0\tissued-tickets/dual-aid\t../veh0-ticket\n"
                ),
                "empty.tsv": (
                    b"# no mappings\n\n"
                ),
            }

            for name, content in mapping_fixtures.items():
                (mapping_dir / name).write_bytes(content)

            result = subprocess.run(
                [str(self.binary), str(fixture), str(mapping_dir)],
                cwd=self.root,
                env=self.env,
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )

            output = result.stdout + result.stderr
            print(output, end="", flush=True)

            self.assertEqual(result.returncode, 0, msg=output)

            for marker in (
                "DISCOVERY_SORT_ORDER=PASS",
                "DISCOVERY_IDENTITY_PRESERVED=PASS",
                "DISCOVERY_PATHS=PASS",
                "SYMLINK_CREDENTIAL_FILES_REJECTED=PASS",
                "MISSING_PREFIX_REJECTED=PASS",
                "EMPTY_INPUTS_REJECTED=PASS",
                "LEGACY_WRAPPER_COMPATIBILITY=PASS",
                "MAPPING_VALID_ENTRIES=PASS",
                "MAPPING_INVALID_INPUTS_REJECTED=PASS",
                "MAPPING_CONTRACT=PASS",
                "PATH_RESOLUTION_VALID_DIRECTORY=PASS",
                "PATH_RESOLUTION_SYMLINK_ESCAPE_REJECTED=PASS",
                "PATH_RESOLUTION_INVALID_INPUTS_REJECTED=PASS",
                "PATH_RESOLUTION_CONTRACT=PASS",
                "PKI_CONFIG_RUN_ONLY_REJECTED=PASS",
                "PKI_CONFIG_MAP_ONLY_REJECTED=PASS",
                "PKI_CONFIG_VALID_PAIR_ACCEPTED=PASS",
                "PKI_CONFIG_CONTRACT=PASS",
                "PKI_CONFIG_FAILED_RECONFIG_REJECTED=PASS",
                "STRICT_UNMAPPED_VEHICLE_REJECTED=PASS",
                "STRICT_MISSING_CREDENTIALS_REJECTED=PASS",
                "STRICT_INVALID_CREDENTIALS_REJECTED=PASS",
                "STRICT_NO_LOCAL_KEY_FALLBACK=PASS",
                "STRICT_INITIALIZATION_CONTRACT=PASS",
                "DISCOVERY_CONTRACT=PASS",
            ):
                self.assertIn(marker, output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
