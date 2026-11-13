#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace ns3
{

enum class TxCompletionStatus
{
    Success,
    Failure
};

struct TxCompletionResult
{
    TxCompletionStatus status{TxCompletionStatus::Failure};
    std::int64_t completedAtSimUs{0};
    std::string reason;
};

class TxCompletion final
{
public:
    using Callback =
        std::function<void(const TxCompletionResult&)>;

    explicit TxCompletion(Callback callback = {});

    TxCompletion(const TxCompletion&) = delete;
    TxCompletion& operator=(const TxCompletion&) = delete;

    // Returns true only for the first terminal result.
    bool Complete(TxCompletionResult result);

    bool IsCompleted() const;

private:
    mutable std::mutex m_mutex;
    bool m_completed{false};
    Callback m_callback;
    TxCompletionResult m_result{};
};

} // namespace ns3
