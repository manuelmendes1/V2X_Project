#include "sumo_adapter.h"

#include <charconv>
#include <cstdint>
#include <system_error>

#include "sumo_parser.h"
#include "vanetza_integration/denm/scenario/sumo_denm_event_parser.h"
#include "sumo_serializer.h"
#include "sumo_socket.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/core/v2x_log.h"

#include <netinet/in.h>

#include <string>
#include <utility>

// ============================================================================
// Static members
// ============================================================================

int SumoAdapter::m_socketFd = -1;

sockaddr_in SumoAdapter::m_localAddr{};

sockaddr_in SumoAdapter::m_sumoAddr{};

SumoAdapter::VehicleUpdateCallback
SumoAdapter::m_vehicleCallback = nullptr;

SumoAdapter::VehicleRemoveCallback
SumoAdapter::m_vehicleRemoveCallback = nullptr;

SumoAdapter::PersonUpdateCallback
SumoAdapter::m_personCallback = nullptr;

SumoAdapter::DenmEventCallback
SumoAdapter::m_denmEventCallback = nullptr;

SumoAdapter::TrafficLightUpdateCallback
SumoAdapter::m_trafficLightCallback = nullptr;
SumoAdapter::StepCompleteCallback SumoAdapter::m_stepCompleteCallback;


// ============================================================================
// Init
// ============================================================================

bool
SumoAdapter::Init(
    int localPort,
    const std::string& sumoIp,
    int sumoPort)
{
    return SumoSocket::Init(
        m_socketFd,
        m_localAddr,
        m_sumoAddr,
        localPort,
        sumoIp,
        sumoPort);
}

// ============================================================================
// Poll
// ============================================================================

void
SumoAdapter::Poll()
{
    if (m_socketFd < 0) {
        return;
    }

    char buffer[512];

    sockaddr_in sender{};

    socklen_t senderLen =
        sizeof(sender);

    while (true) {

        const int bytes =
            SumoSocket::Receive(
                m_socketFd,
                buffer,
                sizeof(buffer) - 1,
                sender,
                senderLen);

        if (bytes <= 0) {
            break;
        }

        buffer[bytes] = '\0';

        const std::string message(buffer, bytes);

        constexpr char stepPrefix[] = "SUMO_STEP_END,";
        if (message.rfind(stepPrefix, 0) == 0)
        {
            const std::string timestampText =
                message.substr(sizeof(stepPrefix) - 1);

            if (timestampText.empty())
            {
                continue;
            }

            std::uint64_t timestampUs = 0;
            const auto parsed = std::from_chars(
                timestampText.data(),
                timestampText.data() + timestampText.size(),
                timestampUs);

            if (parsed.ec != std::errc{} ||
                parsed.ptr != timestampText.data() + timestampText.size())
            {
                continue;
            }

            if (m_stepCompleteCallback)
            {
                m_stepCompleteCallback(timestampUs);
            }

            continue;
        }

        if (message.rfind("SUMO_TLS,", 0) == 0)
        {
            const auto trafficLight =
                ns3::SumoTrafficLightParser::Parse(message);

            if (!trafficLight)
            {
                continue;
            }

            if (m_trafficLightCallback)
            {
                m_trafficLightCallback(*trafficLight);
            }

            continue;
        }

        if (message.rfind("DENM_EVENT,", 0) == 0 ||
            message.rfind("DENM_SCENARIO_EVENT,", 0) == 0)
        {
            const auto event =
                ns3::SumoDenmEventParser::Parse(message);

            if (!event)
            {
                continue;
            }

            if (m_denmEventCallback)
            {
                m_denmEventCallback(*event);
            }

            continue;
        }

        if (message.rfind("SUMO_PERSON,", 0) == 0)
        {
            const auto person =
                ns3::ParseSumoPersonDatagram(message);

            if (!person)
            {
                continue;
            }

            if (m_personCallback)
            {
                m_personCallback(*person);
            }

            continue;
        }

        if (message.rfind("SUMO_VEHICLE,REMOVE,", 0) == 0)
        {
            const auto removal =
                ns3::ParseSumoVehicleRemoveDatagram(message);

            if (!removal)
            {
                continue;
            }

            if (m_vehicleRemoveCallback)
            {
                m_vehicleRemoveCallback(*removal);
            }

            continue;
        }

        auto vehicle =
            ns3::SumoParser::Parse(buffer);

        if (!vehicle) {
            continue;
        }

        // ---------------------------------------------------------------------
        // SUMO state
        //
        // O parser produz um VehicleState temporário contendo apenas:
        //   - vehId
        //   - mobility
        //
        // O SumoAdapter não resolve nem modifica nodeId/stationId.
        // A resolução da identidade permanente é responsabilidade da camada
        // de sincronização/VehicleFactory.
        // ---------------------------------------------------------------------

        V2XWriteLog(
            SimulationContext::simulationStep,
            V2XLogLevel::DEBUG,
            "SUMO",
            "RX_VEHICLE_STATE",
            vehicle->identity.vehId,
            vehicle->identity.nodeId,
            vehicle->identity.stationId,
            -1,
            "pos=("
                + std::to_string(
                    vehicle->mobility.position.x)
                + ","
                + std::to_string(
                    vehicle->mobility.position.y)
                + ") speed="
                + std::to_string(vehicle->mobility.speed)
                + " sumo_sim_time_us="
                + (vehicle->sumoSimulationTimeUs.has_value()
                    ? std::to_string(*vehicle->sumoSimulationTimeUs)
                    : std::string("UNAVAILABLE")));

        // ---------------------------------------------------------------------
        // Forward vehicle state
        //
        // O callback recebe o estado temporário produzido pelo SUMO.
        // A camada seguinte decide:
        //
        //   1. se o veículo já existe;
        //   2. se precisa ser criado;
        //   3. qual VehicleState permanente deve ser atualizado.
        //
        // Desta forma o SumoAdapter não possui autoridade sobre a identidade.
        // ---------------------------------------------------------------------

        if (m_vehicleCallback) {
            m_vehicleCallback(*vehicle);
        }
    }
}

// ============================================================================
// Send
// ============================================================================

void
SumoAdapter::SendToSumo(
    const ns3::VehicleState& vehicle)
{
    if (m_socketFd < 0) {
        return;
    }

    SumoSocket::Send(
        m_socketFd,
        m_sumoAddr,
        SumoSerializer::Serialize(vehicle));

    // TODO:
    // Quando a transmissão utilizar PacketContext desde o início
    // da pipeline, este log deverá migrar para a API baseada em
    // PacketContext.
}

// ============================================================================
// Callback
// ============================================================================

void
SumoAdapter::SetDenmEventCallback(
    DenmEventCallback callback)
{
    m_denmEventCallback =
        std::move(callback);
}

void
SumoAdapter::SetVehicleUpdateCallback(
    VehicleUpdateCallback callback)
{
    m_vehicleCallback =
        std::move(callback);
}

void
SumoAdapter::SetVehicleRemoveCallback(VehicleRemoveCallback callback)
{
    m_vehicleRemoveCallback = std::move(callback);
}

void
SumoAdapter::SetPersonUpdateCallback(PersonUpdateCallback callback)
{
    m_personCallback = std::move(callback);
}

void
SumoAdapter::SetTrafficLightUpdateCallback(
    TrafficLightUpdateCallback callback)
{
    m_trafficLightCallback =
        std::move(callback);
}

void
SumoAdapter::SetStepCompleteCallback(StepCompleteCallback callback)
{
    m_stepCompleteCallback = std::move(callback);
}

// ============================================================================
// Close
// ============================================================================

void
SumoAdapter::Close()
{
    SumoSocket::Close(m_socketFd);
}
