#pragma once

#include <cstddef>
#include <deque>
#include <utility>

#include "vanetza_integration/cpm/types/cpm_message.h"

namespace ns3
{

// Bounded application-facing queue for validated, neutral CPM messages.
// The application consumes messages through VanetzaRuntime::TryReceiveCpm().
class CpmReceptionQueue
{
public:
    static constexpr std::size_t kCapacity = 256;

    bool CanAccept() const noexcept
    {
        return m_messages.size() < kCapacity;
    }

    bool TryPush(const CpmMessage& message)
    {
        if (!CanAccept())
        {
            return false;
        }

        m_messages.push_back(message);
        return true;
    }

    bool TryPop(CpmMessage& message)
    {
        if (m_messages.empty())
        {
            return false;
        }

        message = std::move(m_messages.front());
        m_messages.pop_front();
        return true;
    }

    std::size_t Size() const noexcept
    {
        return m_messages.size();
    }

    bool Empty() const noexcept
    {
        return m_messages.empty();
    }

private:
    std::deque<CpmMessage> m_messages;
};

} // namespace ns3
