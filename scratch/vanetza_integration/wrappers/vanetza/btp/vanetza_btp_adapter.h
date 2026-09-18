#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"
#include "btp_tx_request.h"

namespace vanetza {

    namespace btp {
        class PortDispatcher;
    }

    namespace geonet {
        class Router;
        struct ManagementInformationBase;
        using MIB = ManagementInformationBase;

    } // namespace geonet

} // namespace vanetza

namespace ns3 {

    class VanetzaBtpAdapter {

        public:

            struct RxData {

                uint16_t destinationPort = 0;

                std::vector<uint8_t> payload;

                bool hasSourcePosition = false;
                double latitude = 0.0;
                double longitude = 0.0;

                bool hasTrafficClass = false;
                uint8_t trafficClass = 0;

                bool hasItsAid = false;
                uint32_t itsAid = 0;

                bool hasPermissions = false;
                std::size_t permissionsSize = 0;

                bool hasLifetime = false;
                double lifetimeMs = 0.0;

                bool hasSourceMid = false;
                std::array<uint8_t, 6> sourceMid{};
            };

            using RxHandler = std::function<void(const RxData&)>;

            VanetzaBtpAdapter(vanetza::geonet::Router& router, const vanetza::geonet::MIB& mib);

            ~VanetzaBtpAdapter();

            VanetzaBtpAdapter(const VanetzaBtpAdapter&) = delete;
            VanetzaBtpAdapter& operator=(const VanetzaBtpAdapter&) = delete;

            bool SendBtpB(
                PacketContext& context,
                const BtpTxRequest& request,
                const std::vector<uint8_t>& payload);

            // Compatibility entry point for the existing CAM path.
            bool SendBtpB(
                PacketContext& context,
                uint16_t destinationPort,
                const std::vector<uint8_t>& payload);

            void SetRxHandler(uint16_t destinationPort, RxHandler handler);

        private:

            class RxIndicationHandler;

            vanetza::geonet::Router& m_router;
            const vanetza::geonet::MIB& m_mib;

            std::unique_ptr<vanetza::btp::PortDispatcher> m_dispatcher;
            std::unique_ptr<RxIndicationHandler> m_rxHandler;

            std::unordered_map<uint16_t, RxHandler> m_handlers;
    };

} // namespace ns3
