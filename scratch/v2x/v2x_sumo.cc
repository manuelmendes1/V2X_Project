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

    bool enableCam = true;
    bool enableDenm = false;
    bool enableSpatem = false;
    uint32_t spatemStationId = 0;
    uint32_t spatemIntersectionId = 0;
    std::string spatemTrafficLightId;
    double spatemLatitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double spatemLongitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double spatemNs3X = 0.0;
    double spatemNs3Y = 0.0;

    double denmRadiusMeters = 1000.0;
    uint64_t denmInitialTxDelayUs = 0;

    std::string denmPkiDir;
    std::string cpmPkiDir;

    std::string itsStartTime;

    bool enableCpm = false;
    uint32_t cpmStationId = 0;
    double cpmLatitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double cpmLongitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double cpmNs3X = 0.0;
    double cpmNs3Y = 0.0;
    double cpmPerceptionRangeMeters = 50.0;

    CommandLine cmd;

    cmd.AddValue(
        "simDuration",
        "Duracao maxima da simulacao em segundos (cenario SUMO + margem)",
        simDurationSec);

    cmd.AddValue(
        "enableCam",
        "Ativa o scheduler de transmissão CAM",
        enableCam);

    cmd.AddValue(
        "enableDenm",
        "Ativa o baseline TX DENM",
        enableDenm);

    cmd.AddValue(
        "denmRadius",
        "Raio GBC do baseline DENM em metros",
        denmRadiusMeters);

    cmd.AddValue(
        "denmInitialTxDelayUs",
        "Atraso inicial de transmissao DENM em microssegundos (default: 0)",
        denmInitialTxDelayUs);

    cmd.AddValue(
        "denmPkiDir",
        "Diretorio contendo os certificados e chaves ETSI ITS Security V3 para DENM",
        denmPkiDir);

    cmd.AddValue(
        "itsStartTime",
        "Tempo absoluto inicial do dominio temporal ITS em UTC "
        "(formatos aceites: YYYY-MM-DDTHH:MM:SS ou "
        "YYYY-MM-DD HH:MM:SS; sem sufixo Z ou offset)",
        itsStartTime);

    cmd.AddValue("enableCpm",
        "Ativa a construção de CpmInput; sem transmissão", enableCpm);

    cmd.AddValue(
        "cpmPkiDir",
        "Diretório com certificados e chave ETSI ITS Security V3 da RSU CPM",
        cpmPkiDir);
    cmd.AddValue("cpmStationId",
        "stationId da RSU CPM", cpmStationId);
    cmd.AddValue("cpmLatitudeDeg",
        "Latitude WGS84 da RSU CPM", cpmLatitudeDeg);
    cmd.AddValue("cpmLongitudeDeg",
        "Longitude WGS84 da RSU CPM", cpmLongitudeDeg);
    cmd.AddValue("cpmPerceptionRangeMeters",
        "Alcance horizontal de perceção em metros",
        cpmPerceptionRangeMeters);
    cmd.AddValue("cpmNs3X", "Coordenada X ns-3 da RSU emissora de CPM", cpmNs3X);
    cmd.AddValue("cpmNs3Y", "Coordenada Y ns-3 da RSU emissora de CPM", cpmNs3Y);

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

    cmd.AddValue(
        "spatemNs3X",
        "Coordenada X física da RSU no referencial SUMO/ns-3, em metros",
        spatemNs3X);

    cmd.AddValue(
        "spatemNs3Y",
        "Coordenada Y física da RSU no referencial SUMO/ns-3, em metros",
        spatemNs3Y);

    cmd.Parse(argc, argv);

    if (enableCpm && cpmPkiDir.empty())
    {
        std::cerr << "[CPM_SECURITY] CPM_PKIDIR_REQUIRED: "
                     "--cpmPkiDir is mandatory when --enableCpm=true\n";
        return 2;
    }

    SimulationController::ConfigureSpatemEmitter(
        enableSpatem,
        spatemStationId,
        spatemIntersectionId,
        spatemTrafficLightId,
        spatemLatitudeDeg,
        spatemLongitudeDeg,
        spatemNs3X,
        spatemNs3Y);

    SimulationContext::itsStartTime = itsStartTime;
    SimulationContext::cpmPkiDirectory = cpmPkiDir;

    if (!SimulationController::ConfigureCpmPerception(
            enableCpm, cpmStationId, cpmLatitudeDeg,
            cpmLongitudeDeg,
            cpmNs3X, cpmNs3Y,
            cpmPerceptionRangeMeters))
    {
        std::cerr << "[CPM_PERCEPTION] invalid configuration; startup aborted\n";
        return 2;
    }

    YansWifiChannelHelper ch;

    ch.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    ch.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(2.7), "ReferenceDistance", DoubleValue(1.0));

    channel = ch.Create();

    ipv4.SetBase("10.0.0.0","255.255.255.0");

    VehicleFactory::Initialize(channel, ipv4, MakeCallback(&RxCallback::Receive));

    SumoAdapter::Init(7400,"127.0.0.1",7000);

    SimulationController::ConfigureSumoAdapterCallbacks();

    SimulationController::SetCamEnabled(enableCam);
    SimulationController::SetDenmEnabled(enableDenm);

    SimulationController::SetDenmRadiusMeters(denmRadiusMeters);
    SimulationController::SetDenmInitialTxDelayUs(denmInitialTxDelayUs);

    SimulationController::SetDenmPkiDirectory(denmPkiDir);

    SimulationController::Start(MilliSeconds(100));

    Simulator::Schedule(Seconds(simDurationSec),&SimulationController::StopSimulation);

    Simulator::Run();

    // Release CPM and SPATEM while ns-3 and the station registry are alive.
    SimulationController::ShutdownCpmEmitter();
    SimulationController::ShutdownSpatemEmitter();

    Simulator::Destroy();

    SumoAdapter::Close();

    return 0;
}
