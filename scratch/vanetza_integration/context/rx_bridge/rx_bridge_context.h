#pragma once

#include <cstddef>
#include <vector>

#include "vanetza_integration/context/packet/packet_context.h"

namespace ns3 {

    class RxBridgeContext {

        public:

            class Scope
            {
            public:
                explicit Scope(PacketContext* context);
                ~Scope() noexcept;

                Scope(const Scope&) = delete;
                Scope& operator=(const Scope&) = delete;

                Scope(Scope&&) = delete;
                Scope& operator=(Scope&&) = delete;

            private:
                std::size_t m_depth;
            };

            static void Push(PacketContext* context);

            static PacketContext* Current();

            static void Pop() noexcept;

        private:

            static thread_local std::vector<PacketContext*> s_contexts;
    };

} // namespace ns3
