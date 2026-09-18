#pragma once

#include <cstddef>
#include <string>

#include <netinet/in.h>

// ============================================================================
// SumoSocket
//
// Thin wrapper around the operating system UDP socket API.
//
// This module is responsible only for the socket lifecycle and low-level UDP
// communication between ns-3 and the external SUMO bridge.
//
// Design principles:
//   - Stateless implementation.
//   - No ownership of simulation objects.
//   - No knowledge of CAM, DENM or ETSI messages.
//   - No packet processing.
//   - No business logic.
//
// Higher-level modules are responsible for message serialization,
// deserialization and protocol handling.
// ============================================================================

class SumoSocket{

public:

    // ========================================================================
    // Socket lifecycle
    //
    // Creates, configures and releases the UDP socket used by the SUMO
    // communication adapter.
    // ========================================================================

    static bool Init(int& socketFd, sockaddr_in& localAddr, sockaddr_in& sumoAddr, int localPort, const std::string& sumoIp, int sumoPort);

    static void Close(int& socketFd);

    // ========================================================================
    // UDP Input / Output
    //
    // Thin wrappers around the operating system UDP API.
    //
    // Receive() performs a non-blocking read.
    // Send() transmits a complete UDP datagram.
    // ========================================================================

    static int Receive(int socketFd, char* buffer, std::size_t bufferSize, sockaddr_in& sender, socklen_t& senderLen);

    static bool Send(int socketFd, const sockaddr_in& destination, const std::string& message);
};
