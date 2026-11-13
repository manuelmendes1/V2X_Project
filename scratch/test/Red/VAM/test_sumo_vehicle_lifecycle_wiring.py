#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


def read(relative_path):
    return (ROOT / relative_path).read_text(encoding="utf-8")


def section(source, start, end):
    first = source.find(start)
    if first < 0:
        raise AssertionError(f"SECTION_START_MISSING: {start}")

    last = source.find(end, first + len(start))
    if last < 0:
        raise AssertionError(f"SECTION_END_MISSING: {end}")

    return source[first:last]


class TestSumoVehicleLifecycleWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.registry_h = read(
            "scratch/vanetza_integration/context/"
            "sumo_vehicle_lifecycle_registry/"
            "sumo_vehicle_lifecycle_registry.h"
        )
        cls.registry_cc = read(
            "scratch/vanetza_integration/context/"
            "sumo_vehicle_lifecycle_registry/"
            "sumo_vehicle_lifecycle_registry.cc"
        )
        cls.adapter_h = read(
            "scratch/vanetza_integration/sumo_adapter/sumo_adapter.h"
        )
        cls.adapter_cc = read(
            "scratch/vanetza_integration/sumo_adapter/sumo_adapter.cc"
        )
        cls.controller_h = read(
            "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.h"
        )
        cls.controller_cc = read(
            "scratch/vanetza_integration/context/"
            "simulation_controller/simulation_controller.cc"
        )
        cls.dispatcher_cc = read(
            "scratch/vanetza_integration/context/"
            "vehicle_dispatcher/vehicle_dispatcher.cc"
        )
        cls.export_cc = read(
            "scratch/vanetza_integration/context/"
            "simulation_export/simulation_export.cc"
        )
        cls.cmake = read("scratch/CMakeLists.txt")

    def test_lifecycle_registry_exposes_shared_instance(self):
        self.assertRegex(
            self.registry_h,
            r"static\s+SumoVehicleLifecycleRegistry\s*&\s*Instance\s*\(",
        )
        self.assertIn(
            "SumoVehicleLifecycleRegistry::Instance()",
            self.registry_cc,
        )

    def test_adapter_declares_vehicle_remove_callback(self):
        self.assertIn(
            "using VehicleRemoveCallback",
            self.adapter_h,
        )
        self.assertIn(
            "SetVehicleRemoveCallback",
            self.adapter_h,
        )
        self.assertIn(
            "m_vehicleRemoveCallback",
            self.adapter_h,
        )
        self.assertIn(
            "SumoVehicleRemoveDatagram",
            self.adapter_h,
        )

    def test_adapter_routes_remove_before_generic_vehicle_parser(self):
        remove_branch = self.adapter_cc.find(
            'message.rfind("SUMO_VEHICLE,REMOVE,", 0)'
        )
        generic_parser = self.adapter_cc.find(
            "ns3::SumoParser::Parse(buffer)"
        )

        self.assertGreaterEqual(
            remove_branch,
            0,
            "SUMO_VEHICLE_REMOVE_ROUTE_MISSING",
        )
        self.assertGreater(
            generic_parser,
            remove_branch,
            "REMOVE must be handled before the generic vehicle parser",
        )

        branch = section(
            self.adapter_cc,
            'message.rfind("SUMO_VEHICLE,REMOVE,", 0)',
            "auto vehicle =",
        )
        self.assertIn("ParseSumoVehicleRemoveDatagram(message)", branch)
        self.assertIn("m_vehicleRemoveCallback", branch)

    def test_adapter_defines_remove_callback_setter_and_storage(self):
        self.assertIn(
            "SumoAdapter::VehicleRemoveCallback",
            self.adapter_cc,
        )
        self.assertRegex(
            self.adapter_cc,
            r"SumoAdapter::SetVehicleRemoveCallback\s*\(",
        )

    def test_controller_registers_remove_callback(self):
        configuration = section(
            self.controller_cc,
            "SimulationController::ConfigureSumoAdapterCallbacks()",
            "SimulationController::HandleTrafficLightUpdateFromSumo(",
        )
        self.assertIn(
            "SetVehicleRemoveCallback",
            configuration,
        )
        self.assertIn(
            "&SimulationController::HandleVehicleRemoveFromSumo",
            configuration,
        )
        self.assertIn(
            "HandleVehicleRemoveFromSumo",
            self.controller_h,
        )

    def test_controller_gates_upserts_before_mobility_processing(self):
        handler = section(
            self.controller_cc,
            "SimulationController::HandleVehicleUpdateFromSumo(",
            "SimulationController::HandlePersonUpdateFromSumo(",
        )

        apply_index = handler.find(".ApplyUpsert(")
        process_index = handler.find("SimulationSync::ProcessVehicle(")

        self.assertGreaterEqual(
            apply_index,
            0,
            "VEHICLE_UPSERT_LIFECYCLE_GATE_MISSING",
        )
        self.assertGreater(
            process_index,
            apply_index,
            "mobility must only be processed after lifecycle acceptance",
        )
        self.assertIn("sumoSimulationTimeUs", handler)
        self.assertIn("return;", handler)

    def test_controller_applies_remove_without_erasing_vehicle_state(self):
        self.assertIn(
            "SimulationController::HandleVehicleRemoveFromSumo(",
            self.controller_cc,
        )
        handler = section(
            self.controller_cc,
            "SimulationController::HandleVehicleRemoveFromSumo(",
            "SimulationController::HandlePersonUpdateFromSumo(",
        )
        self.assertIn(".ApplyRemove(", handler)
        self.assertNotIn("VehicleRegistry", handler)
        self.assertNotIn("VehicleFactory", handler)

    def test_controller_clears_lifecycle_at_simulation_start(self):
        start = section(
            self.controller_cc,
            "SimulationController::Start(Time stepLength)",
            "SimulationController::StopSimulation()",
        )
        self.assertIn(
            "SumoVehicleLifecycleRegistry::Instance().Clear()",
            start,
        )

    def test_dispatcher_excludes_inactive_sumo_vehicles(self):
        self.assertIn(
            "sumo_vehicle_lifecycle_registry.h",
            self.dispatcher_cc,
        )
        self.assertIn(
            "IsDispatchEligible(vehId)",
            self.dispatcher_cc,
        )
        self.assertLess(
            self.dispatcher_cc.find("IsDispatchEligible(vehId)"),
            self.dispatcher_cc.find("callback(vehicle)"),
        )

    def test_export_excludes_inactive_sumo_vehicles(self):
        self.assertIn(
            "sumo_vehicle_lifecycle_registry.h",
            self.export_cc,
        )
        self.assertIn(
            "IsDispatchEligible(vehId)",
            self.export_cc,
        )
        self.assertLess(
            self.export_cc.find("IsDispatchEligible(vehId)"),
            self.export_cc.find("SumoAdapter::SendToSumo(*state)"),
        )

    def test_lifecycle_registry_source_is_in_existing_object_library(self):
        expected = (
            "context/sumo_vehicle_lifecycle_registry/"
            "sumo_vehicle_lifecycle_registry.cc"
        )
        self.assertIn(expected, self.cmake)


if __name__ == "__main__":
    unittest.main(verbosity=2)
