import unittest
from pathlib import Path


def repository_root(start: Path) -> Path:
    for candidate in (start.resolve(), *start.resolve().parents):
        if (
            (candidate / "ns3").is_file()
            and (candidate / "scratch" / "CMakeLists.txt").is_file()
        ):
            return candidate
    raise RuntimeError("Raiz do repositorio ns-3 nao encontrada")


class TestItsStationCmakeWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = repository_root(Path(__file__))
        cls.cmake = (cls.root / "scratch/CMakeLists.txt").read_text()

    def test_neutral_its_station_object_library_owns_the_builders(self):
        start = self.cmake.find(
            "add_library(vanetza_its_station_obj OBJECT"
        )
        self.assertGreaterEqual(
            start, 0, "VANETZA_ITS_STATION_OBJECT_LIBRARY_MISSING"
        )
        end = self.cmake.find(
            "target_include_directories(vanetza_its_station_obj", start
        )
        self.assertGreater(
            end, start, "VANETZA_ITS_STATION_INCLUDE_CONFIGURATION_MISSING"
        )
        library_block = self.cmake[start:end]

        for source in (
            "context/its_station/network/its_station_network_builder.cc",
            "context/its_station/address/its_station_address_builder.cc",
        ):
            with self.subTest(source=source):
                self.assertIn(source, library_block)

    def test_spatem_object_library_no_longer_owns_neutral_builders(self):
        start = self.cmake.find("add_library(vanetza_spatem_obj OBJECT")
        end = self.cmake.find(
            "target_include_directories(vanetza_spatem_obj", start
        )
        self.assertGreaterEqual(start, 0, "SPATEM_OBJECT_LIBRARY_MISSING")
        self.assertGreater(end, start, "SPATEM_OBJECT_LIBRARY_BLOCK_INVALID")
        spatem_block = self.cmake[start:end]

        self.assertNotIn(
            "its_station_network_builder.cc",
            spatem_block,
            "SPATEM_STILL_OWNS_NEUTRAL_NETWORK_BUILDER",
        )
        self.assertNotIn(
            "its_station_address_builder.cc",
            spatem_block,
            "SPATEM_STILL_OWNS_NEUTRAL_ADDRESS_BUILDER",
        )

    def test_production_executable_links_the_neutral_object_library(self):
        self.assertIn(
            "$<TARGET_OBJECTS:vanetza_its_station_obj>",
            self.cmake,
            "ITS_STATION_OBJECT_LIBRARY_NOT_LINKED_TO_PRODUCTION_EXECUTABLE",
        )

    def test_service_consumers_use_neutral_include_paths(self):
        expected = (
            (
                "scratch/vanetza_integration/cpm/emitter/cpm_emitter_endpoint.h",
                "context/its_station/state/its_station_state.h",
            ),
            (
                "scratch/vanetza_integration/cpm/emitter/cpm_emitter_endpoint.cc",
                "context/its_station/network/its_station_network_builder.h",
            ),
            (
                "scratch/vanetza_integration/spatem/emitter/spatem_emitter_endpoint.h",
                "context/its_station/state/its_station_state.h",
            ),
            (
                "scratch/vanetza_integration/spatem/emitter/spatem_emitter_endpoint.h",
                "context/its_station/network/its_station_network_builder.h",
            ),
            (
                "scratch/vanetza_integration/spatem/emitter/spatem_rsu_runtime.cc",
                "context/its_station/address/its_station_address_builder.h",
            ),
            (
                "scratch/vanetza_integration/wrappers/vanetza/access/ns3_access_adapter.h",
                "context/its_station/state/its_station_state.h",
            ),
        )

        for relative_path, include_path in expected:
            with self.subTest(file=relative_path):
                source = (self.root / relative_path).read_text()
                self.assertIn(include_path, source)
                self.assertNotIn("spatem/emitter/its_station_", source)

    def test_neutral_foundation_headers_exist_in_the_agreed_layout(self):
        expected = (
            "scratch/vanetza_integration/context/its_station/state/its_station_state.h",
            "scratch/vanetza_integration/context/its_station/address/its_station_address_builder.h",
            "scratch/vanetza_integration/context/its_station/address/its_station_address_builder.cc",
            "scratch/vanetza_integration/context/its_station/network/its_station_network_builder.h",
            "scratch/vanetza_integration/context/its_station/network/its_station_network_builder.cc",
        )
        for relative_path in expected:
            with self.subTest(path=relative_path):
                self.assertTrue(
                    (self.root / relative_path).is_file(),
                    f"NEUTRAL_ITS_STATION_FILE_MISSING: {relative_path}",
                )


    def test_station_identity_authority_is_compiled_by_neutral_object_library(self):
        start = self.cmake.find(
            "add_library(vanetza_its_station_obj OBJECT"
        )
        end = self.cmake.find(
            "target_include_directories(vanetza_its_station_obj", start
        )
        self.assertGreaterEqual(
            start, 0, "ITS_STATION_OBJECT_LIBRARY_MISSING"
        )
        self.assertGreater(
            end, start, "ITS_STATION_OBJECT_LIBRARY_BLOCK_INVALID"
        )

        library_block = self.cmake[start:end]
        expected_source = (
            "context/its_station_identity/"
            "its_station_identity_authority.cc"
        )
        self.assertIn(
            expected_source,
            library_block,
            "ITS_STATION_ID_AUTHORITY_SOURCE_NOT_IN_NEUTRAL_LIBRARY",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
