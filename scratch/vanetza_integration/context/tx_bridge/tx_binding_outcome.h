#pragma once

#include <cstdint>
#include <string>

#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"

namespace ns3
{

void ResolveTxBindingFailure(
    TxBridgeBinding& binding,
    int64_t completedAtSimUs,
    const std::string& reason);

void ResolveTxBindingDrop(
    TxBridgeBinding& binding,
    int64_t completedAtSimUs,
    const std::string& contextReason,
    const std::string& completionReason);

} // namespace ns3
