#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SOURCE = (
    ROOT
    / "vanetza_integration/context/simulation_controller/"
      "simulation_controller.cc"
)


class TestSumoVruSubjectAudit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        start = cls.source.index(
            "SimulationController::HandlePersonUpdateFromSumo("
        )
        end = cls.source.index(
            "const SumoPersonRegistry&",
            start,
        )
        cls.handler = cls.source[start:end]

    def test_audit_exposes_source_vclass_and_resolved_profile(self):
        self.assertIn(
            '" sumo_vclass="',
            self.handler,
            "SUMO_VRU_SUBJECT_AUDIT_MISSING_SOURCE_VCLASS",
        )
        self.assertIn(
            '" vru_profile="',
            self.handler,
            "SUMO_VRU_SUBJECT_AUDIT_MISSING_RESOLVED_PROFILE",
        )

    def test_profile_is_read_from_subject_registry_after_sync(self):
        sync = self.handler.find(
            "SumoVruSubjectSynchronizer::SynchronizeAccepted"
        )
        lookup = self.handler.find(
            "m_sumoVruSubjectRegistry.Find(datagram.personId)"
        )

        self.assertGreaterEqual(sync, 0, "Synchronizer call missing")
        self.assertGreater(
            lookup,
            sync,
            "Profile must be read after accepted synchronization",
        )

    def test_profile_name_conversion_covers_declared_profiles(self):
        for profile in (
            "Unknown",
            "Pedestrian",
            "BicyclistLightVruVehicle",
            "Motorcyclist",
            "Animal",
        ):
            self.assertIn(
                f"VruProfile::{profile}",
                self.source,
                f"Profile conversion missing {profile}",
            )

    def test_audit_uses_stdout_independent_of_ns3_logging_configuration(self):
        self.assertRegex(
            self.handler,
            r'std::cout\s*<<\s*"\[SUMO_PERSON_AUDIT\] ACCEPTED"',
            "SUMO_VRU_SUBJECT_AUDIT_MUST_NOT_USE_COMPILE_DISABLED_NS_LOG",
        )

    def test_audit_remains_opt_in(self):
        self.assertIn("V2X_SUMO_PERSON_AUDIT", self.handler)


if __name__ == "__main__":
    unittest.main(verbosity=2)
