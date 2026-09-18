#pragma once

#include "ns3/node.h"
#include "ns3/socket.h"

#include "vanetza_integration/context/vehicle_state.h"

namespace ns3{

class VehicleBuilder{

public:

    static VehicleState* Create(const std::string& vehId, Ptr<Node> node, Ptr<Socket> socket);

private:

    static VehicleState* InitializeVehicleState(const std::string& vehId, Ptr<Node> node, Ptr<Socket> socket);

    static void LogVehicleCreation(const VehicleState& state);
};

} // namespace ns3
