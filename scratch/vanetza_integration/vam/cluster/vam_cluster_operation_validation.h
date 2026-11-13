#pragma once

#include <string>

#include "vanetza_integration/vam/types/vam_domain_types.h"

namespace vanetza_integration
{
namespace vam
{

class VamClusterOperationValidation
{
public:
    static bool Validate(
        const VamClusterOperation& operation,
        std::string& error);
};

} // namespace vam
} // namespace vanetza_integration
