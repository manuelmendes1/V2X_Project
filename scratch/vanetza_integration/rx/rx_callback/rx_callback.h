#pragma once

#include "ns3/socket.h"

namespace ns3 {

    class RxCallback {

        public:

            static void Receive(Ptr<Socket> socket);
    };

} // namespace ns3
