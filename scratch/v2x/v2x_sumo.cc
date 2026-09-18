
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"

#include <string>

#include "vanetza_integration/rx/rx_callback/rx_callback.h"
#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NS3V2X_CAM");

Ipv4AddressHelper ipv4;
Ptr<YansWifiChannel> channel;

int main(int argc, char* argv[]) {

    GlobalValue::Bind("SimulatorImplementationType",StringValue("ns3::RealtimeSimulatorImpl"));

    double simDurationSec = 60.0;

    bool enableDenm = false;

    double denmRadiusMeters = 1000.0;

    CommandLine cmd;

    cmd.AddValue(
        "simDuration",
        "Duracao maxima da simulacao em segundos (cenario SUMO + margem)",
        simDurationSec);

    cmd.AddValue(
        "enableDenm",
        "Ativa o baseline TX DENM",
        enableDenm);

    cmd.AddValue(
        "denmRadius",
        "Raio GBC do baseline DENM em metros",
        denmRadiusMeters);

    cmd.Parse(argc, argv);

    YansWifiChannelHelper ch;

    ch.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    ch.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(2.7), "ReferenceDistance", DoubleValue(1.0));

    channel = ch.Create();

    ipv4.SetBase("10.0.0.0","255.255.255.0");

    VehicleFactory::Initialize(channel, ipv4, MakeCallback(&RxCallback::Receive));

    SumoAdapter::Init(7400,"127.0.0.1",7000);

    SumoAdapter::SetVehicleUpdateCallback(&SimulationController::HandleVehicleUpdateFromSumo);

    SimulationController::SetDenmEnabled(enableDenm);

    SimulationController::SetDenmRadiusMeters(denmRadiusMeters);

    SimulationController::Start(MilliSeconds(100));

    Simulator::Schedule(Seconds(simDurationSec),&SimulationController::StopSimulation);

    Simulator::Run();

    Simulator::Destroy();

    SumoAdapter::Close();

    return 0;
}
