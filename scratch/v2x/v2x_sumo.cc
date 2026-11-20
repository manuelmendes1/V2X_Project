#include <limits>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"

#include <optional>
#include <string>
#include <vector>

#include "vanetza_integration/rx/rx_callback/rx_callback.h"
#include "vanetza_integration/context/simulation_controller/simulation_controller.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/sumo_adapter/sumo_adapter.h"
#include "vanetza_integration/sumo_adapter/sumo_network_projection_loader.h"
#include "vanetza_integration/context/its_station_identity/its_station_identity_authority.h"
#include "vanetza_integration/vam/scenario/vam_scenario_subjects.h"

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
    std::string sumoNetFile;
    std::optional<SumoUtmWgs84ProjectionConfig> sumoProjection;

    bool enableCpm = false;
    uint32_t cpmStationId = 0;
    double cpmLatitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double cpmLongitudeDeg = std::numeric_limits<double>::quiet_NaN();
    double cpmNs3X = 0.0;
    double cpmNs3Y = 0.0;
    double cpmPerceptionRangeMeters = 50.0;

    bool enableVamGeneration = false;
    bool enableVamEvaluationScenario = false;
    uint32_t vamTCheckVamGenMs = 100;
    uint32_t vamTGenVamMinMs = 100;
    uint32_t vamTGenVamMaxMs = 5000;
    uint32_t vamTGenVamLfMinMs = 2000;

    double vamVehicleSafeLateralMeters =
        std::numeric_limits<double>::quiet_NaN();
    double vamVehicleSafeLongitudinalMeters =
        std::numeric_limits<double>::quiet_NaN();
    double vamVehicleSafeVerticalMeters =
        std::numeric_limits<double>::quiet_NaN();

    double vamVruSafeLateralMeters =
        std::numeric_limits<double>::quiet_NaN();
    double vamVruSafeLongitudinalMeters =
        std::numeric_limits<double>::quiet_NaN();
    double vamVruSafeVerticalMeters =
        std::numeric_limits<double>::quiet_NaN();

    uint64_t vamMaximumSampleAgeUs = 250000;

    // Explicit scenario-owned VRU identities and authorization.
    // Populate with real SUMO person IDs and externally assigned ITS station IDs.
    // Example entry shape:
    // {"<SUMO-person-id>", <unique-station-id>, true,
    //  VamVruScenarioSubjectConfig::VbsState::ActiveStandalone}
    std::vector<VamVruScenarioSubjectConfig> vamScenarioSubjects{};

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

    cmd.AddValue(
        "sumoNetFile",
        "Ficheiro de rede SUMO (.net.xml) com projeção UTM/WGS84",
        sumoNetFile);

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


    cmd.AddValue(
        "enableVamGeneration",
        "Ativa a avaliação de candidatos VAM; não transmite VAM",
        enableVamGeneration);

    cmd.AddValue(
        "enableVamEvaluationScenario",
        "Ativa o sujeito VAM sintético de avaliação; requer enableVamGeneration",
        enableVamEvaluationScenario);

    cmd.AddValue(
        "vamTCheckVamGenMs",
        "Cadência lógica de avaliação VAM em milissegundos",
        vamTCheckVamGenMs);

    cmd.AddValue(
        "vamTGenVamMinMs",
        "Intervalo mínimo de geração VAM em milissegundos",
        vamTGenVamMinMs);

    cmd.AddValue(
        "vamTGenVamMaxMs",
        "Intervalo máximo de geração VAM em milissegundos",
        vamTGenVamMaxMs);

    cmd.AddValue(
        "vamTGenVamLfMinMs",
        "Intervalo mínimo do contentor VAM de baixa frequência",
        vamTGenVamLfMinMs);

    cmd.AddValue(
        "vamVehicleSafeLateralMeters",
        "Limiar lateral de proximidade segura para veículos",
        vamVehicleSafeLateralMeters);

    cmd.AddValue(
        "vamVehicleSafeLongitudinalMeters",
        "Limiar longitudinal de proximidade segura para veículos",
        vamVehicleSafeLongitudinalMeters);

    cmd.AddValue(
        "vamVehicleSafeVerticalMeters",
        "Limiar vertical de proximidade segura para veículos",
        vamVehicleSafeVerticalMeters);

    cmd.AddValue(
        "vamVruSafeLateralMeters",
        "Limiar lateral de proximidade segura para outros VRUs",
        vamVruSafeLateralMeters);

    cmd.AddValue(
        "vamVruSafeLongitudinalMeters",
        "Limiar longitudinal de proximidade segura para outros VRUs",
        vamVruSafeLongitudinalMeters);

    cmd.AddValue(
        "vamVruSafeVerticalMeters",
        "Limiar vertical de proximidade segura para outros VRUs",
        vamVruSafeVerticalMeters);

    cmd.AddValue(
        "vamMaximumSampleAgeUs",
        "Idade máxima aceite para observações SUMO em microssegundos",
        vamMaximumSampleAgeUs);

    cmd.Parse(argc, argv);

    // Load the optional network projection exactly once, before configuring
    // the VAM scheduler. An explicitly supplied but invalid file fails
    // startup; omission preserves Cartesian-only VAM evaluation.
    if (!sumoNetFile.empty())
    {
        SumoUtmWgs84ProjectionConfig projection{};
        std::string projectionError;

        if (!SumoNetworkProjectionLoader::LoadUtmWgs84(
                sumoNetFile,
                projection,
                projectionError))
        {
            std::cerr
                << "[SUMO_PROJECTION] invalid network projection; "
                   "startup aborted: "
                << projectionError << '\n';
            return 2;
        }

        sumoProjection = projection;

        std::cout
            << "[SUMO_PROJECTION] loaded"
            << " net_file=" << sumoNetFile
            << " utm_zone=" << projection.zone
            << " hemisphere="
            << (projection.northernHemisphere ? "north" : "south")
            << " net_offset_x_m=" << projection.netOffsetXMeters
            << " net_offset_y_m=" << projection.netOffsetYMeters
            << '\n';
    }

    if (enableVamEvaluationScenario && !enableVamGeneration)
    {
        std::cerr
            << "[VAM_SCENARIO] enableVamEvaluationScenario requires "
               "enableVamGeneration=true\n";
        return 2;
    }

    if (enableVamGeneration && enableVamEvaluationScenario)
    {
        vamScenarioSubjects = MakeVamEvaluationScenarioSubjects();
    }

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


    vanetza_integration::vam::VamGenerationConfig vamGenerationConfig{};
    vamGenerationConfig.tCheckVamGenMs = vamTCheckVamGenMs;
    vamGenerationConfig.tGenVamMinMs = vamTGenVamMinMs;
    vamGenerationConfig.tGenVamMaxMs = vamTGenVamMaxMs;
    vamGenerationConfig.tGenVamLfMinMs = vamTGenVamLfMinMs;

    vanetza_integration::vam::VamGenerationEvaluationAdapterConfig
        vamEvaluationConfig{};

    vamEvaluationConfig.snapshotBuilder.vehicleThresholds = {
        vamVehicleSafeLateralMeters,
        vamVehicleSafeLongitudinalMeters,
        vamVehicleSafeVerticalMeters
    };

    vamEvaluationConfig.snapshotBuilder.vruThresholds = {
        vamVruSafeLateralMeters,
        vamVruSafeLongitudinalMeters,
        vamVruSafeVerticalMeters
    };

    vamEvaluationConfig.proximity.maximumSampleAgeUs =
        vamMaximumSampleAgeUs;

    std::string vamConfigurationError;
    if (!SimulationController::ConfigureVamGeneration(
            enableVamGeneration,
            vamGenerationConfig,
            vamEvaluationConfig,
            vamConfigurationError,
            sumoProjection))
    {
        std::cerr
            << "[VAM_GENERATION] invalid configuration; startup aborted: "
            << vamConfigurationError << '\n';
        return 2;
    }

    std::string vamScenarioSubjectError;
    if (!SimulationController::ConfigureVamVruScenarioSubjects(vamScenarioSubjects,
            vamScenarioSubjectError))
    {
        std::cerr
            << "[VAM_SCENARIO] invalid subject configuration; startup aborted: "
            << vamScenarioSubjectError << '\n';
        return 2;
    }

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

    // VAM must drain before any subsystem teardown or global identity clear.
    if (!SimulationController::ShutdownVamGeneration())
    {
        std::cerr
            << "[VAM_GENERATION] shutdown blocked; normal teardown aborted; "
               "ITS identity authority was not explicitly cleared\n";
        return 3;
    }

    // Other runtimes are released only after VAM transaction drain succeeds.
    SimulationController::ShutdownCpmEmitter();
    SimulationController::ShutdownSpatemEmitter();

    Simulator::Destroy();

    SumoAdapter::Close();

    // Release simulation-scoped ITS station identities after shutdown.
    ItsStationIdentityAuthority::Instance().Clear();

    return 0;
}
