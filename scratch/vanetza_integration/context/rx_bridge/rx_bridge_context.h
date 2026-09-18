#pragma once

#include "vanetza_integration/context/packet/packet_context.h"

namespace ns3 {

    class RxBridgeContext {

        public:

            static void Push(PacketContext* context);

            static PacketContext* Current();

            static void Pop();

        private:

            static thread_local PacketContext* s_context;
    };

} // namespace ns3
