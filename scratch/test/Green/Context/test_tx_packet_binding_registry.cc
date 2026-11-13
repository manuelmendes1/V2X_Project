#include "vanetza_integration/context/tx_bridge/tx_packet_binding_registry.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

namespace
{

bool Check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << name << '\n';
    return condition;
}

bool Contains(
    const std::vector<ns3::TxBridgeBinding>& bindings,
    const std::shared_ptr<ns3::PacketContext>& context,
    const std::shared_ptr<ns3::TxCompletion>& completion)
{
    return std::any_of(
        bindings.begin(),
        bindings.end(),
        [&](const ns3::TxBridgeBinding& binding)
        {
            return binding.context == context &&
                   binding.completion == completion;
        });
}

} // namespace

int main()
{
    using namespace ns3;

    bool passed = true;

    TxPacketBindingRegistry registry;

    int packetKey1 = 1;
    int packetKey2 = 2;
    int packetKey3 = 3;

    auto context1 = std::make_shared<PacketContext>();
    auto completion1 = std::make_shared<TxCompletion>();

    passed &= Check(
        registry.Store(&packetKey1, {context1, completion1}),
        "FIRST_PACKET_BINDING_STORED");

    passed &= Check(
        registry.Size() == 1,
        "REGISTRY_SIZE_UPDATED");

    const TxBridgeBinding found =
        registry.Find(&packetKey1);

    passed &= Check(
        found.context == context1 &&
            found.completion == completion1,
        "FIND_RETURNS_COMPLETE_BINDING");

    // A duplicate key must not overwrite the original transaction.
    auto contextReplacement =
        std::make_shared<PacketContext>();
    auto completionReplacement =
        std::make_shared<TxCompletion>();

    passed &= Check(
        !registry.Store(
            &packetKey1,
            {contextReplacement, completionReplacement}),
        "DUPLICATE_PACKET_KEY_REJECTED");

    const TxBridgeBinding afterDuplicate =
        registry.Find(&packetKey1);

    passed &= Check(
        afterDuplicate.context == context1 &&
            afterDuplicate.completion == completion1,
        "DUPLICATE_DOES_NOT_REPLACE_BINDING");

    // Take transfers both shared references and erases the entry.
    const TxBridgeBinding taken =
        registry.Take(&packetKey1);

    passed &= Check(
        taken.context == context1 &&
            taken.completion == completion1,
        "TAKE_TRANSFERS_COMPLETE_BINDING");

    passed &= Check(
        registry.Size() == 0 &&
            !registry.Find(&packetKey1).context &&
            !registry.Find(&packetKey1).completion,
        "TAKE_REMOVES_PACKET_ENTRY");

    passed &= Check(
        !registry.Take(&packetKey1).context &&
            !registry.Take(&packetKey1).completion,
        "REPEATED_TAKE_RETURNS_EMPTY");

    // Legacy context-only bindings remain representable.
    auto context2 = std::make_shared<PacketContext>();

    passed &= Check(
        registry.Store(&packetKey2, {context2, nullptr}),
        "CONTEXT_ONLY_BINDING_STORED");

    // A completion-only binding must also remain representable.
    auto completion3 = std::make_shared<TxCompletion>();

    passed &= Check(
        registry.Store(&packetKey3, {nullptr, completion3}),
        "COMPLETION_ONLY_BINDING_STORED");

    const auto drained = registry.Drain();

    passed &= Check(
        drained.size() == 2,
        "DRAIN_RETURNS_ALL_PENDING_BINDINGS");

    passed &= Check(
        Contains(drained, context2, nullptr) &&
            Contains(drained, nullptr, completion3),
        "DRAIN_PRESERVES_EACH_BINDING_PAIR");

    passed &= Check(
        registry.Size() == 0 &&
            !registry.Find(&packetKey2).context &&
            !registry.Find(&packetKey3).completion,
        "DRAIN_CLEARS_REGISTRY");

    passed &= Check(
        !registry.Store(nullptr, {context1, completion1}),
        "NULL_PACKET_KEY_REJECTED");

    if (!passed)
    {
        std::cout << "TX_PACKET_BINDING_REGISTRY=FAIL\n";
        return 1;
    }

    std::cout << "TX_PACKET_BINDING_REGISTRY=PASS\n";
    return 0;
}
