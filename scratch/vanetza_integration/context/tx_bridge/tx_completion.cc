#include "tx_completion.h"

#include <utility>

namespace ns3
{

TxCompletion::TxCompletion(Callback callback)
    : m_callback(std::move(callback))
{
}

bool
TxCompletion::Complete(TxCompletionResult result)
{
    Callback callback;
    TxCompletionResult completedResult;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_completed)
        {
            return false;
        }

        m_completed = true;
        m_result = std::move(result);

        completedResult = m_result;
        callback = std::move(m_callback);
    }

    if (callback)
    {
        callback(completedResult);
    }

    return true;
}

bool
TxCompletion::IsCompleted() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_completed;
}

} // namespace ns3
