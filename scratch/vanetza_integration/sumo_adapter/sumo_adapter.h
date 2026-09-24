#pragma once

#include <functional>
#include <string>

#include <netinet/in.h>

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"

class SumoAdapter{

public:

    using VehicleUpdateCallback = std::function<void(const ns3::VehicleState&)>;
    using DenmEventCallback = std::function<void(const ns3::DenmScenarioEvent&)>;

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
    static void SetDenmEventCallback(DenmEventCallback callback);

private:

    // =====================================================
    // Callback
    // =====================================================

    static int m_socketFd;

    static sockaddr_in m_localAddr;

    static sockaddr_in m_sumoAddr;

    static VehicleUpdateCallback m_vehicleCallback;
    static DenmEventCallback m_denmEventCallback;


};
