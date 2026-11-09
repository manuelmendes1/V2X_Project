#!/usr/bin/env python3
"""RED contract for integer-microsecond SUMO bridge loop boundaries."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
BRIDGE_PATH = ROOT / "sumo/current/integration/v2x_bridge_16.py"

spec = importlib.util.spec_from_file_location("v2x_bridge_16", BRIDGE_PATH)
if spec is None or spec.loader is None:
    raise RuntimeError(f"Cannot load SUMO bridge module: {BRIDGE_PATH}")
bridge = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = bridge
spec.loader.exec_module(bridge)


class TestSumoPersonTimeBoundary(unittest.TestCase):
    def should_continue_us(self, **kwargs):
        helper = getattr(bridge, "should_continue_simulation_us", None)
        self.assertTrue(
            callable(helper),
            "SUMO_INTEGER_TIME_API_MISSING: bridge must expose "
            "should_continue_simulation_us",
        )
        return helper(**kwargs)

    def test_tls_loop_stops_at_exact_integer_microsecond_limit(self):
        self.assertTrue(
            self.should_continue_us(
                min_expected_number=1,
                tls_enabled=True,
                sim_time_us=7_900_000,
                sim_time_limit_us=8_000_000,
            )
        )
        self.assertFalse(
            self.should_continue_us(
                min_expected_number=1,
                tls_enabled=True,
                sim_time_us=8_000_000,
                sim_time_limit_us=8_000_000,
            )
        )

    def test_integer_step_counter_does_not_add_an_extra_tenth_second(self):
        step_length_us = int(round(0.1 * 1_000_000))
        sim_time_us = 0
        sim_time_limit_us = 8_000_000
        iterations = 0

        while self.should_continue_us(
            min_expected_number=1,
            tls_enabled=True,
            sim_time_us=sim_time_us,
            sim_time_limit_us=sim_time_limit_us,
        ):
            iterations += 1
            sim_time_us += step_length_us

        self.assertEqual(iterations, 80)
        self.assertEqual(sim_time_us, 8_000_000)
        self.assertFalse(
            self.should_continue_us(
                min_expected_number=1,
                tls_enabled=True,
                sim_time_us=sim_time_us,
                sim_time_limit_us=sim_time_limit_us,
            )
        )

    def test_non_tls_vehicle_driven_mode_keeps_existing_semantics(self):
        self.assertTrue(
            self.should_continue_us(
                min_expected_number=1,
                tls_enabled=False,
                sim_time_us=8_000_000,
                sim_time_limit_us=8_000_000,
            )
        )
        self.assertFalse(
            self.should_continue_us(
                min_expected_number=0,
                tls_enabled=False,
                sim_time_us=0,
                sim_time_limit_us=8_000_000,
            )
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
