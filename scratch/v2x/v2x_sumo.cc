#include <limits>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"

#include <string>

#include "vanetza_integration/rx/rx_callback/rx_callback.h"
#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
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
    bool enableSpatem = false;
    uint32_t spatemStationId = 0;
    uint32_t spatemIntersectionId = 0;
    std::string spatemTrafficLightId;
    double spatemLatitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double spatemLongitudeDeg = std::numeric_limits<double>::quiet_NaN();

    double denmRadiusMeters = 1000.0;

    std::string denmPkiDir;

    std::string itsStartTime;

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

    cmd.AddValue(
        "denmPkiDir",
        "Diretorio contendo os certificados e chaves ETSI ITS Security V3 para DENM",
        denmPkiDir);

    cmd.AddValue(
        "itsStartTime",
        "Tempo absoluto inicial do dominio temporal ITS "
        "(ISO-8601, por exemplo 2026-09-24T10:01:32)",
        itsStartTime);

    cmd.AddValue(
        "enableSpatem",
        "Ativa a inicialização do endpoint de rede RSU SPATEM",
        enableSpatem);

    cmd.AddValue(
        "spatemStationId",
        "stationId exclusivo da RSU SPATEM",
        spatemStationId);

    cmd.AddValue(
        "spatemIntersectionId",
        "ID da interseção SPATEM (1..65535)",
        spatemIntersectionId);

    cmd.AddValue(
        "spatemTrafficLightId",
        "ID explícito do semáforo SUMO associado à RSU",
        spatemTrafficLightId);

    cmd.AddValue(
        "spatemLatitudeDeg",
        "Latitude geodésica da RSU, em graus (-90..90)",
        spatemLatitudeDeg);

    cmd.AddValue(
        "spatemLongitudeDeg",
        "Longitude geodésica da RSU, em graus (-180..180)",
        spatemLongitudeDeg);

    cmd.Parse(argc, argv);

    SimulationController::ConfigureSpatemEmitter(
        enableSpatem,
        spatemStationId,
        spatemIntersectionId,
        spatemTrafficLightId,
        spatemLatitudeDeg,
        spatemLongitudeDeg);

    SimulationContext::itsStartTime = itsStartTime;

    YansWifiChannelHelper ch;

    ch.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    ch.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(2.7), "ReferenceDistance", DoubleValue(1.0));

    channel = ch.Create();

    ipv4.SetBase("10.0.0.0","255.255.255.0");

    VehicleFactory::Initialize(channel, ipv4, MakeCallback(&RxCallback::Receive));

    SumoAdapter::Init(7400,"127.0.0.1",7000);

    SimulationController::ConfigureSumoAdapterCallbacks();

    SimulationController::SetDenmEnabled(enableDenm);

    SimulationController::SetDenmRadiusMeters(denmRadiusMeters);

    SimulationController::SetDenmPkiDirectory(denmPkiDir);

    SimulationController::Start(MilliSeconds(100));

    Simulator::Schedule(Seconds(simDurationSec),&SimulationController::StopSimulation);

    Simulator::Run();

    Simulator::Destroy();

    SumoAdapter::Close();

    return 0;
}
