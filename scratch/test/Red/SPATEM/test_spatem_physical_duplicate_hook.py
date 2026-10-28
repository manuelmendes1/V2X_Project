#!/usr/bin/env python3
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
SOURCE = (
    ROOT
    / "scratch/vanetza_integration/wrappers/vanetza/access"
      "/ns3_access_adapter.cc"
)


class SpatemPhysicalDuplicateHookTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_duplicate_is_opt_in_and_scoped_to_spatem_aid_137(self):
        self.assertIn(
            'std::getenv("V2X_SPATEM_DUPLICATE")',
            self.source,
        )
        self.assertRegex(
            self.source,
            r"const bool duplicateSpatem\s*=\s*ctx\s*&&"
            r"\s*securedTxAidValid\s*&&"
            r"\s*securedTxAid\s*==\s*137",
        )
        self.assertIn(
            'std::string(spatemDuplicateEnv) == "1"',
            self.source,
        )

    def test_duplicate_copies_the_serialized_packet_and_is_deferred(self):
        self.assertIn(
            "[ctx, socket, ns3Packet, duplicateDenm, duplicateSpatem, duplicateCpm]()",
            self.source,
        )
        self.assertRegex(
            self.source,
            r"if\s*\(duplicateSpatem\)\s*\{"
            r"[\s\S]{0,250}ns3Packet->Copy\(\)",
        )
        self.assertRegex(
            self.source,
            r"if\s*\(duplicateSpatem\)\s*\{[\s\S]{0,700}"
            r"Simulator::Schedule\(\s*MicroSeconds\(100\)",
        )
        self.assertIn("[SPATEM_PHYSICAL_DUPLICATE]", self.source)
        self.assertIn("duplicatePacketId", self.source)

    def test_existing_denm_duplicate_hook_remains_scoped_to_aid_37(self):
        self.assertIn('std::getenv("V2X_DENM_DUPLICATE")', self.source)
        self.assertRegex(
            self.source,
            r"const bool duplicateDenm\s*=\s*ctx\s*&&"
            r"\s*securedTxAidValid\s*&&\s*securedTxAid\s*==\s*37",
        )
        self.assertIn("[DENM_V3_DUPLICATE]", self.source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
