#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <string>

#include <ns3/mac48-address.h>

namespace ns3 {

    struct GeoNetRxContext {

        bool hasSourcePosition = false;

        double latitude = 0.0;
        double longitude = 0.0;

        // -----------------------------------------------------------------------------
        // Sender Link MAC
        //
        // Identificador da camada de enlace ns-3/WiFi.
        // Obtido do remetente resolvido no boundary RX.
        //
        // NÃO representa o GN_ADDR.MID.
        // -----------------------------------------------------------------------------
        ns3::Mac48Address senderLinkMac;
        bool hasSenderLinkMac = false;

        // -----------------------------------------------------------------------------
        // Sender GeoNetworking MID
        //
        // Identificador GN de rede transportado pelo header GeoNetworking.
        //
        // Representado como dados neutros da aplicação.
        // NÃO representa o MAC L2 ns-3.
        // NÃO expõe tipos Vanetza.
        // -----------------------------------------------------------------------------
        std::array<uint8_t, 6> senderMid{};
        bool hasSenderMid = false;

        std::string upperProtocol = "UNKNOWN";
        std::string transportType = "UNKNOWN";

        bool hasLifetime = false;
        double lifetimeMs = 0.0;

        bool hasTrafficClass = false;
        uint8_t trafficClass = 0;

        bool hasItsAid = false;
        uint32_t itsAid = 0;

        bool hasPermissions = false;
        std::size_t permissionsSize = 0;
    };

}
