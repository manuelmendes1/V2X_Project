#pragma once

#include <memory>
#include <vector>

#include <vanetza/access/interface.hpp>

#include "ns3/ptr.h"

namespace vanetza {

    class ChunkPacket;

    namespace geonet {

        struct ManagementInformationBase;
        using MIB = ManagementInformationBase;

        class Router;

    } // namespace geonet

} // namespace vanetza

namespace ns3 {

    struct VehicleState;

    class WifiPhy;

    class Ns3AccessAdapter final : public vanetza::access::Interface {
        public:

            Ns3AccessAdapter(VehicleState& vehicle, const vanetza::geonet::MIB& mib, vanetza::geonet::Router& router);

            ~Ns3AccessAdapter() override = default;

            void request(const vanetza::access::DataRequest& request, std::unique_ptr<vanetza::ChunkPacket> packet) override;

            Ptr<WifiPhy> GetWifiPhy() const;

        private:

            std::vector<uint8_t> SerializePacket(const vanetza::ChunkPacket& packet) const;

            bool ValidateBasicHeaderTx(const std::vector<uint8_t>& networkBytes) const;

            bool ValidateCommonHeaderTx(const std::vector<uint8_t>& networkBytes, std::size_t transportSize, std::size_t applicationSize, std::size_t frameSize) const;

            bool ValidateSecuredMessageV3Tx(const std::vector<uint8_t>& networkBytes) const;

            bool ValidateShbExtendedHeaderTx(const std::vector<uint8_t>& networkBytes) const;

        private:

            VehicleState& m_vehicle;

            const vanetza::geonet::MIB& m_mib;

            vanetza::geonet::Router& m_router;
    };


} // namespace ns3
