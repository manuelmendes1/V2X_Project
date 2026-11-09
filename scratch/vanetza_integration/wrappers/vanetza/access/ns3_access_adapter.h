#pragma once

#include <memory>
#include <vector>

#include <vanetza/access/interface.hpp>

#include "ns3/ptr.h"
#include "ns3/node.h"
#include "ns3/socket.h"

#include "vanetza_integration/context/its_station/state/its_station_state.h"

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

            Ns3AccessAdapter(Ptr<Node> node, Ptr<Socket> socket,
                             const vanetza::geonet::MIB& mib,
                             vanetza::geonet::Router& router);

            Ns3AccessAdapter(VehicleState& vehicle, const vanetza::geonet::MIB& mib, vanetza::geonet::Router& router);

            Ns3AccessAdapter(ItsStationState& station, const vanetza::geonet::MIB& mib, vanetza::geonet::Router& router);

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

            Ptr<Node> m_node;
            Ptr<Socket> m_socket;

            const vanetza::geonet::MIB& m_mib;

            vanetza::geonet::Router& m_router;
    };


} // namespace ns3
