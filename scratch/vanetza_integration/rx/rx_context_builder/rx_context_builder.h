#pragma once


#include "vanetza_integration/context/packet/packet_context.h"

namespace ns3 {

    class VehicleState;

}

class RxContextBuilder {

    public:

        static ns3::PacketContext Build(
            uint16_t destinationPort,
            const ns3::VehicleState& receiver);
};
