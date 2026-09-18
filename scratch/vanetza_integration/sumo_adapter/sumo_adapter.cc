#include "sumo_adapter.h"

#include "sumo_parser.h"
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
                + std::to_string(
                    vehicle->mobility.speed));

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
SumoAdapter::SetVehicleUpdateCallback(
    VehicleUpdateCallback callback)
{
    m_vehicleCallback =
        std::move(callback);
}

// ============================================================================
// Close
// ============================================================================

void
SumoAdapter::Close()
{
    SumoSocket::Close(m_socketFd);
}
