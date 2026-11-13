#include "vanetza_integration/vam/cluster/vam_cluster_operation_validation.h"

#include <chrono>
#include <cstdint>

namespace vanetza_integration
{
namespace vam
{
namespace
{

bool Reject(std::string& error, const char* reason)
{
    error = reason;
    return false;
}

bool IsValidTime(const VamClusterOperationTime& time)
{
    if (!time.duration.has_value())
    {
        // ASN.1 unavailable (255).
        return true;
    }

    const auto milliseconds = time.duration->count();

    return milliseconds >= 250 &&
           milliseconds <= 63500 &&
           milliseconds % 250 == 0;
}

bool IsValidLeaveReason(VamClusterLeaveReason reason)
{
    switch (reason)
    {
        case VamClusterLeaveReason::NotProvided:
        case VamClusterLeaveReason::ClusterLeaderLost:
        case VamClusterLeaveReason::ClusterDisbandedByLeader:
        case VamClusterLeaveReason::OutOfClusterBoundingBox:
        case VamClusterLeaveReason::OutOfClusterSpeedRange:
        case VamClusterLeaveReason::JoiningAnotherCluster:
        case VamClusterLeaveReason::CancelledJoin:
        case VamClusterLeaveReason::FailedJoin:
        case VamClusterLeaveReason::SafetyCondition:
            return true;
    }

    return false;
}

bool IsValidBreakupReason(VamClusterBreakupReason reason)
{
    switch (reason)
    {
        case VamClusterBreakupReason::NotProvided:
        case VamClusterBreakupReason::ClusteringPurposeCompleted:
        case VamClusterBreakupReason::LeaderMovedOutOfClusterBoundingBox:
        case VamClusterBreakupReason::JoiningAnotherCluster:
        case VamClusterBreakupReason::EnteringLowRiskAreaBasedOnMaps:
        case VamClusterBreakupReason::ReceptionOfCpmContainingCluster:
            return true;
    }

    return false;
}

} // namespace

bool VamClusterOperationValidation::Validate(
    const VamClusterOperation& operation,
    std::string& error)
{
    error.clear();

    if (!operation.clusterJoinInfo.has_value() &&
        !operation.clusterLeaveInfo.has_value() &&
        !operation.clusterBreakupInfo.has_value() &&
        !operation.clusterIdChangeTimeInfo.has_value())
    {
        return Reject(error, "cluster operation container is empty");
    }

    if (operation.clusterJoinInfo.has_value() &&
        !IsValidTime(operation.clusterJoinInfo->joinTime))
    {
        return Reject(error, "cluster join time is invalid");
    }

    if (operation.clusterLeaveInfo.has_value() &&
        !IsValidLeaveReason(
            operation.clusterLeaveInfo->clusterLeaveReason))
    {
        return Reject(error, "cluster leave reason is undefined");
    }

    if (operation.clusterBreakupInfo.has_value())
    {
        if (!IsValidBreakupReason(
                operation.clusterBreakupInfo->clusterBreakupReason))
        {
            return Reject(error, "cluster breakup reason is undefined");
        }

        if (!IsValidTime(operation.clusterBreakupInfo->breakupTime))
        {
            return Reject(error, "cluster breakup time is invalid");
        }
    }

    if (operation.clusterIdChangeTimeInfo.has_value() &&
        !IsValidTime(*operation.clusterIdChangeTimeInfo))
    {
        return Reject(error, "cluster ID change time is invalid");
    }

    return true;
}

} // namespace vam
} // namespace vanetza_integration
