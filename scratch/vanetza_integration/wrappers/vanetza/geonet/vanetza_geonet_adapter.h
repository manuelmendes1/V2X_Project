#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <ns3/mac48-address.h>

#include "vanetza_integration/context/geo_net/geo_net_mid.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"

namespace ns3 {

    class VehicleState;
    struct PacketContext;

    class VanetzaGeoNetAdapter {

        public:

            struct BtpRxData{

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
                GeoNetMid sourceMid{};
            };

            using BtpRxHandler = std::function<void(const BtpRxData&)>;

            explicit VanetzaGeoNetAdapter(VehicleState& vehicle);

            ~VanetzaGeoNetAdapter();

            VanetzaGeoNetAdapter(const VanetzaGeoNetAdapter&) = delete;

            VanetzaGeoNetAdapter& operator=(const VanetzaGeoNetAdapter&) = delete;

            bool Initialize();

            void Shutdown();

            bool IsInitialized() const;

            bool UpdatePosition();

            // Classifies a complete GeoNetworking RX frame using Vanetza's actual parser hierarchy. No wire-format offsets are exposed to the application/runtime layer.

            std::string ClassifyRxGeoNetworking(const std::vector<uint8_t>& buffer) const;

            bool Receive(PacketContext& context, const std::vector<uint8_t>& buffer, const Mac48Address& senderMac);

            bool SendBtpB(
                PacketContext& context,
                const BtpTxRequest& request,
                const std::vector<uint8_t>& payload);

            bool SendBtpB(
                PacketContext& context,
                uint16_t destinationPort,
                const std::vector<uint8_t>& payload);

            void SetBtpRxHandler(uint16_t destinationPort, BtpRxHandler handler);

            bool SynchronizeTimeAndPosition();

            // Reload the DENM signer from the credential currently
            // selected by VehicleSecurityContext.
            bool ReloadDenmCredential();

            const GeoNetMid& LocalMid() const;

        private:

            struct Impl;

            std::unique_ptr<Impl> m_impl;


        };

} // namespace ns3
