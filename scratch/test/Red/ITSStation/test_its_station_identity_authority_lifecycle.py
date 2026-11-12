from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[4]


class TestItsStationIdentityAuthorityLifecycle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main = (
            ROOT / "scratch/v2x/v2x_sumo.cc"
        ).read_text(encoding="utf-8")

    def test_authority_is_cleared_after_runtime_shutdown(self):
        include = (
            '#include "vanetza_integration/context/'
            'its_station_identity/its_station_identity_authority.h"'
        )
        clear = "ItsStationIdentityAuthority::Instance().Clear();"

        self.assertIn(
            include,
            self.main,
            "IDENTITY_AUTHORITY_INCLUDE_MISSING_FROM_MAIN",
        )

        self.assertEqual(
            self.main.count(clear),
            1,
            "MAIN_MUST_CLEAR_IDENTITY_AUTHORITY_EXACTLY_ONCE",
        )

        markers = [
            "Simulator::Run();",
            "SimulationController::ShutdownCpmEmitter();",
            "SimulationController::ShutdownSpatemEmitter();",
            "Simulator::Destroy();",
            "SumoAdapter::Close();",
            clear,
        ]

        positions = [self.main.find(marker) for marker in markers]

        for marker, position in zip(markers, positions):
            self.assertGreaterEqual(
                position,
                0,
                f"SHUTDOWN_MARKER_MISSING: {marker}",
            )

        self.assertEqual(
            positions,
            sorted(positions),
            "IDENTITY_AUTHORITY_MUST_BE_CLEARED_AFTER_ALL_RUNTIME_SHUTDOWN",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
