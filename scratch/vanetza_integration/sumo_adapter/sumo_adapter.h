#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <netinet/in.h>

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"
#include "vanetza_integration/sumo_adapter/sumo_person_datagram.h"
#include "vanetza_integration/sumo_adapter/sumo_vehicle_datagram.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_parser.h"

class SumoAdapter{

public:

    using VehicleUpdateCallback = std::function<void(const ns3::VehicleState&)>;
    using VehicleRemoveCallback = std::function<void(const ns3::SumoVehicleRemoveDatagram&)>;
    using PersonUpdateCallback = std::function<void(const ns3::SumoPersonDatagram&)>;
    using DenmEventCallback = std::function<void(const ns3::DenmScenarioEvent&)>;
    using TrafficLightUpdateCallback =
        std::function<void(const ns3::SumoTrafficLightState&)>;
    using StepCompleteCallback = std::function<void(std::uint64_t)>;

    // =====================================================
    // Lifecycle
    // =====================================================

    static bool Init(int localPort, const std::string& sumoIp,int sumoPort);

    static void Close();

    // =====================================================
    // Runtime
    // =====================================================

    static void Poll();

    static void SendToSumo(const ns3::VehicleState& vehicle);

    static void SetVehicleUpdateCallback(VehicleUpdateCallback callback);
    static void SetVehicleRemoveCallback(VehicleRemoveCallback callback);
    static void SetPersonUpdateCallback(PersonUpdateCallback callback);
    static void SetDenmEventCallback(DenmEventCallback callback);
    static void SetTrafficLightUpdateCallback(
        TrafficLightUpdateCallback callback);
    static void SetStepCompleteCallback(StepCompleteCallback callback);

private:

    // =====================================================
    // Callback
    // =====================================================

    static int m_socketFd;

    static sockaddr_in m_localAddr;

    static sockaddr_in m_sumoAddr;

    static VehicleUpdateCallback m_vehicleCallback;
    static VehicleRemoveCallback m_vehicleRemoveCallback;
    static PersonUpdateCallback m_personCallback;
    static DenmEventCallback m_denmEventCallback;
    static TrafficLightUpdateCallback m_trafficLightCallback;
    static StepCompleteCallback m_stepCompleteCallback;


};
