#pragma once

#include "ns3/nstime.h"

namespace ns3 {

    class CamScheduler {

        public:
            static void Run();

            static Time RunInterval();
    };

} // namespace ns3
