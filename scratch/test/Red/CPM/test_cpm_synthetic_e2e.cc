#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <vanetza/common/clock.hpp>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/wifi-module.h"

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/packet/packet_id_manager.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_network/vehicle_network_builder.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"
#include "vanetza_integration/cpm/emitter/cpm_emitter_endpoint.h"
#include "vanetza_integration/cpm/emitter/cpm_rsu_runtime.h"
#include "vanetza_integration/cpm/perception/cpm_synthetic_perception_source.h"
#include "vanetza_integration/rx/rx_callback/rx_callback.h"

#include "vanetza_integration/context/its_time/its_start_time_parser.h"

namespace
{
using namespace ns3;

constexpr std::uint32_t kSenderStationId = 9001;
constexpr char kReceiverId[] = "cpm-synthetic-e2e-receiver";

std::unique_ptr<CpmEmitterEndpoint> g_senderEndpoint;
std::unique_ptr<CpmRsuRuntime> g_senderRuntime;
VehicleState* g_receiver = nullptr;

CpmInput g_expectedInput{};
std::vector<std::uint8_t> g_expectedUper;
std::shared_ptr<PacketContext> g_txContext;

std::int64_t g_referenceTimeItsMs = 0;
bool g_delivered = false;
std::string g_failure;

std::uint64_t Fnv1a64(const std::vector<std::uint8_t>& bytes)
{
    std::uint64_t hash = 14695981039346656037ULL;

    for (const std::uint8_t byte : bytes)
    {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }

    return hash;
}

std::string Fingerprint(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::hex
        << std::setw(16)
        << std::setfill('0')
        << Fnv1a64(bytes);
    return out.str();
}

std::string CurrentUtcItsStartTime()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm utc{};
    if (gmtime_r(&time, &utc) == nullptr)
    {
        throw std::runtime_error("Unable to obtain UTC clock");
    }

    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

void FailAndStop(const std::string& reason)
{
    if (g_failure.empty())
    {
        g_failure = reason;
    }

    std::cerr << "CPM_SYNTHETIC_E2E_FAIL=" << reason << '\n';
    Simulator::Stop();
}

bool BuildKnownInput()
{
    CpmSyntheticPerceptionScenario scenario{};
    scenario.stationId = kSenderStationId;
    scenario.stationPosition.xMeters = 0.0;
    scenario.stationPosition.yMeters = 0.0;
    scenario.stationPosition.zMeters = 0.0;
    scenario.referenceTimeItsMs = g_referenceTimeItsMs;
    scenario.perceptionRangeMeters = 50.0;
    scenario.knownObjectCount = 2;

    CpmSyntheticObjectObservation included{};
    included.objectId = 501;
    included.position.xMeters = 12.5;
    included.position.yMeters = -3.25;
    included.measurementTimeItsMs = g_referenceTimeItsMs - 50;
    included.firstObservedTimeItsMs = g_referenceTimeItsMs - 1050;

    CpmSyntheticObjectObservation outOfRange{};
    outOfRange.objectId = 502;
    outOfRange.position.xMeters = 100.0;
    outOfRange.position.yMeters = 100.0;
    outOfRange.measurementTimeItsMs = g_referenceTimeItsMs - 40;
    outOfRange.firstObservedTimeItsMs = g_referenceTimeItsMs - 900;

    scenario.objects = {included, outOfRange};

    std::string error;
    if (!CpmSyntheticPerceptionSource::Build(
            scenario,
            g_expectedInput,
            error))
    {
        FailAndStop("synthetic source rejected known input: " + error);
        return false;
    }

    if (g_expectedInput.station.stationId != kSenderStationId ||
        g_expectedInput.referenceTimeItsMs != g_referenceTimeItsMs ||
        g_expectedInput.knownObjectCount != 2 ||
        g_expectedInput.perceivedObjects.size() != 1 ||
        g_expectedInput.perceivedObjects.front().objectId != 501)
    {
        FailAndStop("synthetic source did not produce the expected fixture");
        return false;
    }

    // Coordenadas geográficas explícitas da RSU de teste.
    // Correspondem à posição usada na inicialização de CpmRsuRuntime;
    // não são inferidas das coordenadas locais ENU da fonte sintética.
    g_expectedInput.referencePosition.latitudeDeg = 0.0;
    g_expectedInput.referencePosition.longitudeDeg = 0.0;

    const auto encoded = CpmUperEncoder::Encode(g_expectedInput);
    if (!encoded.success || encoded.bytes.empty())
    {
        FailAndStop(
            "independent UPER reference encoding failed: " +
            encoded.error);
        return false;
    }

    g_expectedUper = encoded.bytes;
    return true;
}

void SendKnownCpm()
{
    if (!BuildKnownInput())
    {
        return;
    }

    if (!g_senderRuntime || !g_senderRuntime->IsInitialized())
    {
        FailAndStop("sender runtime is not initialized");
        return;
    }

    g_txContext = std::make_shared<PacketContext>();
    g_txContext->simulationStep = 0;
    g_txContext->simulationTime = Simulator::Now().GetSeconds();
    g_txContext->timestamp =
        static_cast<std::uint64_t>(Simulator::Now().GetMicroSeconds());
    g_txContext->senderNodeId = g_senderEndpoint->GetNode()->GetId();
    g_txContext->senderStationId = g_senderEndpoint->StationId();
    g_txContext->packetId = PacketIdManager::Instance().NextPacketId();
    g_txContext->sequenceId = PacketIdManager::Instance().NextSequenceId(
        g_senderEndpoint->LogicalId(),
        PayloadType::CPM);

    if (!g_senderRuntime->Send(g_txContext, g_expectedInput, 0))
    {
        FailAndStop("CpmRsuRuntime::Send returned false");
        return;
    }

    // PacketContext is shared through the TX pipeline. At this point,
    // payload.bytes may already hold the serialized GeoNetworking frame.
    // Compare UPER fingerprints at ENCODE and BTP_RX instead.
    if (g_txContext->packetId == 0 || g_expectedUper.empty())
    {
        FailAndStop(
            "TX context identity or independent UPER reference is missing");
        return;
    }

    std::cout
        << "[CPM_SYNTHETIC_E2E_TX]"
        << " packet_id=" << g_txContext->packetId
        << " station_id=" << g_txContext->senderStationId
        << " reference_time_its_ms=" << g_expectedInput.referenceTimeItsMs
        << " known_objects=" << static_cast<unsigned>(g_expectedInput.knownObjectCount)
        << " included_objects=" << g_expectedInput.perceivedObjects.size()
        << " uper_bytes=" << g_expectedUper.size()
        << " payload_fnv1a64=" << Fingerprint(g_expectedUper)
        << '\n';
}

bool ValidateDeliveredMessage(const CpmMessage& message)
{
    if (message.station.stationId != kSenderStationId)
    {
        FailAndStop("received CPM stationId mismatch");
        return false;
    }

    if (!std::holds_alternative<CpmRoadSideUnitStationData>(
            message.station.origin))
    {
        FailAndStop("received CPM does not identify an RSU origin");
        return false;
    }

    if (message.referenceTimeItsMs != g_referenceTimeItsMs)
    {
        FailAndStop("received CPM reference time mismatch");
        return false;
    }

    if (message.knownObjectCount != 2 ||
        message.perceivedObjects.size() != 1)
    {
        FailAndStop("received CPM known/included object counts mismatch");
        return false;
    }

    const auto& object = message.perceivedObjects.front();

    if (object.objectId != 501)
    {
        FailAndStop("received CPM objectId mismatch");
        return false;
    }

    if (object.measurementDeltaTimeMs != -50 ||
        !object.objectAgeMs.has_value() ||
        *object.objectAgeMs != 1050)
    {
        FailAndStop("received CPM temporal attributes missing or mismatched");
        return false;
    }

    if (std::abs(object.position.xEast.value - 12.5) > 0.25 ||
        std::abs(object.position.yNorth.value + 3.25) > 0.25)
    {
        FailAndStop("received CPM ENU coordinates mismatch");
        return false;
    }

    std::cout
        << "[CPM_SYNTHETIC_E2E_RX]"
        << " receiver_station_id=" << g_receiver->identity.stationId
        << " sender_station_id=" << message.station.stationId
        << " reference_time_its_ms=" << message.referenceTimeItsMs
        << " known_objects=" << static_cast<unsigned>(message.knownObjectCount)
        << " included_objects=" << message.perceivedObjects.size()
        << " object_id=" << object.objectId
        << " measurement_delta_ms=" << object.measurementDeltaTimeMs
        << " object_age_ms=" << *object.objectAgeMs
        << " x_east_m=" << object.position.xEast.value
        << " y_north_m=" << object.position.yNorth.value
        << '\n';

    std::cout << "CPM_SYNTHETIC_E2E_SEMANTICS=PASS\n";

    // A message can enter this application queue only after the real RX
    // runtime, security gate, UPER decoder, semantic validator and replay
    // check have accepted the packet. The harness never inserts a message
    // into CpmReceptionQueue and never calls CpmRxPipeline::Process.
    std::cout << "CPM_SYNTHETIC_E2E_SECURITY=PASS\n";
    return true;
}

void PollForDeliveredCpm()
{
    if (g_receiver == nullptr ||
        !g_receiver->runtime ||
        !g_receiver->runtime->IsInitialized())
    {
        FailAndStop("receiver runtime became unavailable");
        return;
    }

    CpmMessage message{};
    if (g_receiver->runtime->TryReceiveCpm(message))
    {
        if (ValidateDeliveredMessage(message))
        {
            g_delivered = true;
            std::cout << "CPM_SYNTHETIC_E2E=PASS\n";
            Simulator::Stop();
        }
        return;
    }

    if (Simulator::Now() >= Seconds(5.0))
    {
        FailAndStop("no validated CPM delivered before timeout");
        return;
    }

    Simulator::Schedule(MilliSeconds(10), &PollForDeliveredCpm);
}
} // namespace

int main(int argc, char** argv)
{
    using namespace ns3;

    if (argc != 2)
    {
        std::cerr << "Usage: test_cpm_synthetic_e2e <cpm-pki-directory>\n";
        return 2;
    }

    SimulationContext::itsStartTime = CurrentUtcItsStartTime();
    SimulationContext::cpmPkiDirectory = argv[1];
    SimulationContext::simulationStep = 0;

    vanetza::Clock::time_point itsStart{};
    std::string itsStartTimeError;
    if (!ItsStartTimeParser::ParseUtc(
            SimulationContext::itsStartTime,
            itsStart,
            itsStartTimeError))
    {
        std::cerr << "CPM_SYNTHETIC_E2E_FAIL=invalid ITS start time: "
                  << itsStartTimeError << '\n';
        return 3;
    }

    g_referenceTimeItsMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            itsStart.time_since_epoch()).count() + 1000;

    YansWifiChannelHelper channelHelper;
    channelHelper.SetPropagationDelay(
        "ns3::ConstantSpeedPropagationDelayModel");
    channelHelper.AddPropagationLoss(
        "ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue(2.7),
        "ReferenceDistance", DoubleValue(1.0));

    Ptr<YansWifiChannel> channel = channelHelper.Create();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");

    VehicleFactory::Initialize(
        channel,
        ipv4,
        MakeCallback(&RxCallback::Receive));

    Ptr<Node> receiverNode =
        VehicleFactory::CreateVehicle(kReceiverId);

    g_receiver =
        VehicleRegistry::Instance().GetByVehId(kReceiverId);

    if (g_receiver == nullptr ||
        g_receiver->node != receiverNode ||
        !g_receiver->runtime ||
        !g_receiver->runtime->IsInitialized())
    {
        std::cerr << "CPM_SYNTHETIC_E2E_FAIL=receiver initialization failed\n";
        return 3;
    }

    g_receiver->mobility.position = Vector(0.0, 0.0, 0.0);
    Ptr<ConstantVelocityMobilityModel> receiverMobility =
        receiverNode->GetObject<ConstantVelocityMobilityModel>();
    if (receiverMobility != nullptr)
    {
        receiverMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    }

    CpmEmitterConfig emitterConfig{};
    emitterConfig.logicalId = "cpm-synthetic-e2e-rsu";
    emitterConfig.stationId = kSenderStationId;
    emitterConfig.latitudeDeg = 0.0;
    emitterConfig.longitudeDeg = 0.0;
    emitterConfig.ns3X = 10.0;
    emitterConfig.ns3Y = 0.0;

    g_senderEndpoint = CpmEmitterEndpoint::Create(emitterConfig);
    if (!g_senderEndpoint)
    {
        std::cerr << "CPM_SYNTHETIC_E2E_FAIL=RSU endpoint configuration rejected\n";
        return 4;
    }

    if (!VehicleNetworkBuilder::InitializeCpmEmitter(*g_senderEndpoint))
    {
        std::cerr << "CPM_SYNTHETIC_E2E_FAIL=RSU network initialization failed\n";
        return 5;
    }

    g_senderRuntime = std::make_unique<CpmRsuRuntime>(*g_senderEndpoint);
    if (!g_senderRuntime->Initialize())
    {
        std::cerr << "CPM_SYNTHETIC_E2E_FAIL=RSU runtime initialization failed\n";
        g_senderRuntime.reset();
        g_senderEndpoint.reset();
        VehicleRegistry::Instance().Clear();
        Simulator::Destroy();
        return 6;
    }

    std::cout
        << "CPM_SYNTHETIC_E2E_SETUP=PASS"
        << " receiver_station_id=" << g_receiver->identity.stationId
        << " sender_station_id=" << kSenderStationId
        << " its_start_time=" << SimulationContext::itsStartTime
        << '\n';

    // The scenario is driven exclusively by ns-3 simulation time. No SUMO
    // adapter is initialized, and no SUMO callback is used to trigger TX.
    Simulator::Schedule(Seconds(1.0), &SendKnownCpm);
    Simulator::Schedule(Seconds(1.01), &PollForDeliveredCpm);
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();

    const bool passed = g_delivered && g_failure.empty();

    g_senderRuntime.reset();
    g_senderEndpoint.reset();
    VehicleRegistry::Instance().Clear();
    Simulator::Destroy();

    if (!passed)
    {
        if (g_failure.empty())
        {
            std::cerr << "CPM_SYNTHETIC_E2E_FAIL=simulation ended without delivery\n";
        }
        return 1;
    }

    return 0;
}
