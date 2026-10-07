#include "vanetza_integration/sumo_adapter/sumo_adapter.h"
#include "vanetza_integration/sumo_adapter/sumo_traffic_light_parser.h"
#include "vanetza_integration/denm/scenario/denm_scenario_event_source.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <string>

namespace
{

bool
Expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }

    std::cout << "PASS " << message << '\n';
    return true;
}

bool
SendUdp(int port, const std::string& message)
{
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0)
    {
        return false;
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const ssize_t sent = ::sendto(
        fd,
        message.data(),
        message.size(),
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));

    ::close(fd);

    return sent == static_cast<ssize_t>(message.size());
}

} // namespace

int
main()
{
    std::cout
        << "===== SUMO ADAPTER SPATEM SIGNAL ACCEPTANCE =====\n";

    constexpr int localPort = 7414;

    bool ok = true;

    bool vehicleReceived = false;
    bool denmReceived = false;
    bool trafficLightReceived = false;

    ns3::SumoTrafficLightState receivedTls;

    ok &= Expect(
        SumoAdapter::Init(
            localPort,
            "127.0.0.1",
            7000),
        "SUMO adapter initializes");

    SumoAdapter::SetVehicleUpdateCallback(
        [&](const ns3::VehicleState& vehicle)
        {
            vehicleReceived = true;

            ok &= Expect(
                vehicle.identity.vehId == "veh0",
                "existing VEHICLE callback remains intact");
        });

    SumoAdapter::SetDenmEventCallback(
        [&](const ns3::DenmScenarioEvent& event)
        {
            denmReceived = true;

            ok &= Expect(
                event.vehicleId == "veh0",
                "existing DENM callback remains intact");
        });

    SumoAdapter::SetTrafficLightUpdateCallback(
        [&](const ns3::SumoTrafficLightState& state)
        {
            trafficLightReceived = true;
            receivedTls = state;
        });

    ok &= Expect(
        SendUdp(
            localPort,
            "veh0,10.000,20.000,15.000,90.000,0.000"),
        "VEHICLE datagram is sent");

    ok &= Expect(
        SendUdp(
            localPort,
            "DENM_EVENT,veh0,event-0001,DETECTED,"
            "10.000,20.000,81,1000.0,5000,2100000,hard_brake"),
        "DENM_EVENT datagram is sent");

    ok &= Expect(
        SendUdp(
            localPort,
            "SUMO_TLS,B0,0,G,0,30.0,30.0,1,0.0"),
        "SUMO_TLS datagram is sent");

    SumoAdapter::Poll();

    ok &= Expect(
        vehicleReceived,
        "VEHICLE datagram reaches vehicle callback");

    ok &= Expect(
        denmReceived,
        "DENM_EVENT reaches DENM callback");

    ok &= Expect(
        trafficLightReceived,
        "SUMO_TLS reaches traffic-light callback");

    if (trafficLightReceived)
    {
        ok &= Expect(
            receivedTls.trafficLightId == "B0",
            "TLS trafficLightId reaches callback");

        ok &= Expect(
            receivedTls.linkIndex == 0,
            "TLS linkIndex reaches callback");

        ok &= Expect(
            receivedTls.rawState == 'G',
            "TLS rawState reaches callback");

        ok &= Expect(
            receivedTls.phaseIndex == 0,
            "TLS phaseIndex reaches callback");

        ok &= Expect(
            receivedTls.phaseDuration == 30.0,
            "TLS phaseDuration reaches callback");

        ok &= Expect(
            receivedTls.nextSwitch == 30.0,
            "TLS nextSwitch reaches callback");

        ok &= Expect(
            receivedTls.programId == 1,
            "TLS programId reaches callback");

        ok &= Expect(
            receivedTls.simulationTime == 0.0,
            "TLS simulationTime reaches callback");
    }

    SumoAdapter::Close();

    std::cout
        << "SUMO_ADAPTER_SPATEM_SIGNAL_ACCEPTANCE_RC="
        << (ok ? 0 : 1)
        << '\n';

    return ok ? 0 : 1;
}
