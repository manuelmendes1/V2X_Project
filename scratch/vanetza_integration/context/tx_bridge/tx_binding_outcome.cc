#include "tx_binding_outcome.h"

namespace ns3
{

void ResolveTxBindingFailure(
    TxBridgeBinding& binding,
    int64_t completedAtSimUs,
    const std::string& reason)
{
    // PacketContext outcome is independent of optional completion notification.
    if (binding.context)
    {
        binding.context->MarkFailure();
        binding.context->reason = reason;
    }

    if (binding.completion)
    {
        binding.completion->Complete({
            TxCompletionStatus::Failure,
            completedAtSimUs,
            reason
        });
    }
}

void ResolveTxBindingDrop(
    TxBridgeBinding& binding,
    int64_t completedAtSimUs,
    const std::string& contextReason,
    const std::string& completionReason)
{
    // Context diagnostics and completion diagnostics are separate contracts.
    if (binding.context)
    {
        binding.context->MarkDrop();
        binding.context->reason = contextReason;
    }

    if (binding.completion)
    {
        binding.completion->Complete({
            TxCompletionStatus::Failure,
            completedAtSimUs,
            completionReason
        });
    }
}

} // namespace ns3
