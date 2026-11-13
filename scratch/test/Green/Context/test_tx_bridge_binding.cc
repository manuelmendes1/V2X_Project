#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/context/tx_bridge/tx_completion.h"

#include <iostream>
#include <memory>

namespace
{

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << name << '\n';
    return condition;
}

} // namespace

int main()
{
    using namespace ns3;

    bool passed = true;

    auto context = std::make_shared<PacketContext>();
    auto completion = std::make_shared<TxCompletion>();

    TxBridgeContext::Push(context, completion);

    passed &= Check(
        TxBridgeContext::Current() == context,
        "BRIDGE_CURRENT_CONTEXT_PRESERVED");

    passed &= Check(
        TxBridgeContext::CurrentBinding().completion == completion,
        "BRIDGE_COMPLETION_PUBLISHED");

    const TxBridgeBinding binding = TxBridgeContext::Take();

    passed &= Check(
        binding.context == context &&
            binding.completion == completion,
        "TAKE_TRANSFERS_CONTEXT_AND_COMPLETION");

    passed &= Check(
        TxBridgeContext::Current() == nullptr &&
            TxBridgeContext::CurrentBinding().completion == nullptr,
        "TAKE_CLEARS_BRIDGE_ATOMICALLY");

    // Legacy callers must not inherit a previous completion.
    auto legacyContext = std::make_shared<PacketContext>();
    TxBridgeContext::Push(legacyContext);

    passed &= Check(
        TxBridgeContext::Current() == legacyContext &&
            TxBridgeContext::CurrentBinding().completion == nullptr,
        "LEGACY_PUSH_DOES_NOT_INHERIT_COMPLETION");

    // Pop must clear both members of a published binding.
    TxBridgeContext::Push(context, completion);
    TxBridgeContext::Pop();

    passed &= Check(
        TxBridgeContext::Current() == nullptr &&
            TxBridgeContext::CurrentBinding().completion == nullptr,
        "POP_CLEARS_CONTEXT_AND_COMPLETION");

    // A mismatched context must not consume another transaction.
    auto otherContext = std::make_shared<PacketContext>();
    auto otherCompletion = std::make_shared<TxCompletion>();

    TxBridgeContext::Push(context, completion);

    const TxBridgeBinding wrongContext =
        TxBridgeContext::TakeIfMatches(otherContext, completion);

    passed &= Check(
        !wrongContext.context &&
            TxBridgeContext::CurrentBinding().context == context,
        "MISMATCHED_CONTEXT_LEAVES_BINDING_UNTOUCHED");

    const TxBridgeBinding wrongCompletion =
        TxBridgeContext::TakeIfMatches(context, otherCompletion);

    passed &= Check(
        !wrongCompletion.context &&
            TxBridgeContext::CurrentBinding().completion == completion,
        "MISMATCHED_COMPLETION_LEAVES_BINDING_UNTOUCHED");

    const TxBridgeBinding exactMatch =
        TxBridgeContext::TakeIfMatches(context, completion);

    passed &= Check(
        exactMatch.context == context &&
            exactMatch.completion == completion &&
            TxBridgeContext::Current() == nullptr,
        "EXACT_MATCH_TAKES_AND_CLEARS_BINDING");

    // The identity-checked operation also supports legacy context-only use.
    TxBridgeContext::Push(legacyContext);

    const TxBridgeBinding legacyMatch =
        TxBridgeContext::TakeIfMatches(legacyContext, nullptr);

    passed &= Check(
        legacyMatch.context == legacyContext &&
            !legacyMatch.completion &&
            TxBridgeContext::Current() == nullptr,
        "LEGACY_CONTEXT_ONLY_BINDING_CAN_BE_TAKEN");

    // Taking an empty bridge must return an empty binding.
    const TxBridgeBinding empty = TxBridgeContext::Take();

    passed &= Check(
        !empty.context && !empty.completion,
        "EMPTY_TAKE_RETURNS_EMPTY_BINDING");

    if (!passed)
    {
        std::cout << "TX_BRIDGE_BINDING=FAIL\n";
        return 1;
    }

    std::cout << "TX_BRIDGE_BINDING=PASS\n";
    return 0;
}
