#include "sumo_socket.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/core/v2x_log.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

bool SumoSocket::Init(int& socketFd,sockaddr_in& localAddr, sockaddr_in& sumoAddr, int localPort, const std::string& sumoIp, int sumoPort){

    // =====================================================
    // Create the UDP socket used by the ns-3 <-> SUMO bridge. The socket remains open for the entire lifetime of the adapter and is released by Close().
    // =====================================================

    socketFd = ::socket(AF_INET, SOCK_DGRAM, 0);

    if (socketFd < 0){

        V2XWriteLog(SimulationContext::simulationStep, V2XLogLevel::ERROR, "SUMO", "SOCKET_FAIL", "-", 0, 0, -1, "Unable to create UDP socket");

        return false;
    }


    // Configure the receive queue before bind() and before SUMO starts
    // sending its UDP stream. Linux may double the requested value.
    const int requestedReceiveBufferBytes = 4 * 1024 * 1024;

    if (::setsockopt(
            socketFd,
            SOL_SOCKET,
            SO_RCVBUF,
            &requestedReceiveBufferBytes,
            sizeof(requestedReceiveBufferBytes)) < 0)
    {
        V2XWriteLog(
            SimulationContext::simulationStep,
            V2XLogLevel::ERROR,
            "SUMO",
            "SO_RCVBUF_FAIL",
            "-",
            0,
            0,
            -1,
            "Unable to configure UDP receive buffer");

        ::close(socketFd);
        socketFd = -1;
        return false;
    }

    int effectiveReceiveBufferBytes = 0;
    socklen_t receiveBufferLength = sizeof(effectiveReceiveBufferBytes);

    if (::getsockopt(
            socketFd,
            SOL_SOCKET,
            SO_RCVBUF,
            &effectiveReceiveBufferBytes,
            &receiveBufferLength) < 0)
    {
        V2XWriteLog(
            SimulationContext::simulationStep,
            V2XLogLevel::ERROR,
            "SUMO",
            "SO_RCVBUF_VERIFY_FAIL",
            "-",
            0,
            0,
            -1,
            "Unable to verify UDP receive buffer");

        ::close(socketFd);
        socketFd = -1;
        return false;
    }

    if (effectiveReceiveBufferBytes < requestedReceiveBufferBytes)
    {
        V2XWriteLog(
            SimulationContext::simulationStep,
            V2XLogLevel::ERROR,
            "SUMO",
            "SO_RCVBUF_TOO_SMALL",
            "-",
            0,
            0,
            -1,
            "Effective UDP receive buffer is smaller than requested");

        ::close(socketFd);
        socketFd = -1;
        return false;
    }

    V2XWriteLog(
        SimulationContext::simulationStep,
        V2XLogLevel::INFO,
        "SUMO",
        "SO_RCVBUF_CONFIGURED",
        "-",
        0,
        0,
        -1,
        "requested_bytes="
            + std::to_string(requestedReceiveBufferBytes)
            + " effective_bytes="
            + std::to_string(effectiveReceiveBufferBytes));

    // =====================================================
    // Configure the local endpoint. The adapter listens on the configured UDP port for messages arriving from the SUMO communication bridge.
    // =====================================================

    std::memset(&localAddr, 0, sizeof(sockaddr_in));

    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(localPort);
    localAddr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(socketFd, reinterpret_cast<sockaddr*>(&localAddr), sizeof(sockaddr_in)) < 0){

        V2XWriteLog(SimulationContext::simulationStep, V2XLogLevel::ERROR, "SUMO", "BIND_FAIL", "-", 0, 0, -1, "Unable to bind UDP socket");

        ::close(socketFd);
        socketFd = -1;

        return false;
    }

    // =====================================================
    // Configure the remote SUMO endpoint. inet_pton() validates the supplied IPv4 address before storing it into the destination socket structure.
    // =====================================================

    std::memset(&sumoAddr, 0, sizeof(sockaddr_in));

    sumoAddr.sin_family = AF_INET;
    sumoAddr.sin_port = htons(sumoPort);

    if (::inet_pton(AF_INET, sumoIp.c_str(), &sumoAddr.sin_addr) != 1){

        V2XWriteLog(SimulationContext::simulationStep, V2XLogLevel::ERROR, "SUMO", "INVALID_IP", "-", 0, 0, -1, "Invalid SUMO IPv4 address");

        ::close(socketFd);
        socketFd = -1;

        return false;
    }

    // =====================================================
    // The UDP communication channel has been successfully configured and is ready to exchange messages.
    // =====================================================

    V2XWriteLog(SimulationContext::simulationStep, V2XLogLevel::INFO, "SUMO", "INIT_OK", "-", 0, 0, -1, "UDP adapter initialized");

    return true;
}

int SumoSocket::Receive(int socketFd, char* buffer, std::size_t bufferSize, sockaddr_in& sender, socklen_t& senderLen){

    // =====================================================
    // Perform a non-blocking receive operation. MSG_DONTWAIT guarantees that the simulation loop never blocks waiting for external SUMO traffic.
    // =====================================================

    return ::recvfrom(socketFd, buffer, bufferSize, MSG_DONTWAIT, reinterpret_cast<sockaddr*>(&sender), &senderLen);
}

bool SumoSocket::Send(int socketFd, const sockaddr_in& destination, const std::string& message){

    // =====================================================
    // A negative descriptor indicates that the socket was never initialized or has already been closed.
    // =====================================================

    if (socketFd < 0)
        return false;

    const ssize_t bytes = ::sendto(socketFd, message.data(), message.size(), 0, reinterpret_cast<const sockaddr*>(&destination), sizeof(sockaddr_in));

    // =====================================================
    // sendto() should transmit the complete message in a single UDP datagram.
    // =====================================================

    return bytes == static_cast<ssize_t>(message.size());
}

void SumoSocket::Close(int& socketFd){

    // =====================================================
    // Release the UDP socket associated with the SUMO
    // communication adapter.
    //
    // Closing an already invalid descriptor is ignored,
    // making this operation safe to call multiple times.
    //
    // After the operating system resource is released, the
    // descriptor is reset to -1 to prevent accidental reuse.
    // =====================================================

    if (socketFd >= 0){

        ::close(socketFd);

        socketFd = -1;

        V2XWriteLog(SimulationContext::simulationStep, V2XLogLevel::INFO, "SUMO", "SOCKET_CLOSED", "-", 0, 0, -1, "UDP adapter socket closed");
    }
}
