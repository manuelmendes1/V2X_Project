#!/usr/bin/env python3
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
SOURCE = (
    ROOT
    / "scratch/vanetza_integration/context/simulation_controller/"
      "simulation_controller.cc"
)


class TestVamScenarioVbsStateContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = SOURCE.read_text()
        start = cls.text.index(
            "SimulationController::ConfigureVamVruScenarioSubjects("
        )
        end = cls.text.index(
            "if (!m_vamGenerationScheduler)", start
        )
        cls.validation = cls.text[start:end]

    def test_only_idle_and_active_standalone_are_accepted(self):
        self.assertIn("case VbsState::Idle:", self.validation)
        self.assertIn("case VbsState::ActiveStandalone:", self.validation)
        self.assertNotIn("case VbsState::ActiveClusterLeader:", self.validation)
        self.assertNotIn("case VbsState::Passive:", self.validation)

    def test_role_disabled_requires_idle(self):
        self.assertIn(
            "if (!subject.activateRole && subject.vbsState != VbsState::Idle)",
            self.validation,
        )

    def test_role_enabled_requires_active_standalone(self):
        self.assertIn(
            "if (subject.activateRole &&",
            self.validation,
        )
        self.assertIn(
            "subject.vbsState != VbsState::ActiveStandalone",
            self.validation,
        )

    def test_cluster_states_are_rejected_with_explanation(self):
        self.assertIn("Unsupported VBS state in VAM scenario configuration", self.validation)
        self.assertIn("ApplyClusterLifecycle()", self.text)


if __name__ == "__main__":
    unittest.main(verbosity=2)
