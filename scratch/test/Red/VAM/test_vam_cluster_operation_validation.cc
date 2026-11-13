#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "vanetza_integration/vam/cluster/vam_cluster_operation_validation.h"

using namespace vanetza_integration::vam;

namespace
{

int failures = 0;

void Check(bool condition, const std::string& description)
{
    if (condition)
    {
        std::cout << "[PASS] " << description << '\n';
    }
    else
    {
        std::cerr << "[FAIL] " << description << '\n';
        ++failures;
    }
}

bool Validate(const VamClusterOperation& operation)
{
    std::string error;
    const bool valid =
        VamClusterOperationValidation::Validate(operation, error);

    if (valid && !error.empty())
    {
        return false;
    }

    if (!valid && error.empty())
    {
        return false;
    }

    return valid;
}

VamClusterOperation LeaveOperation(
    VamClusterId id = VamClusterId{42},
    VamClusterLeaveReason reason = VamClusterLeaveReason::SafetyCondition)
{
    VamClusterOperation operation{};
    operation.clusterLeaveInfo = VamClusterLeaveInfo{id, reason};
    return operation;
}

void TestEmptyOperation()
{
    const VamClusterOperation operation{};

    std::string error;
    const bool accepted =
        VamClusterOperationValidation::Validate(operation, error);

    Check(!accepted && !error.empty(),
          "empty operation container is rejected with an error");
}

void TestIdentifierBoundaries()
{
    Check(
        Validate(LeaveOperation(VamClusterId{0})),
        "cluster identifier zero is accepted");

    Check(
        Validate(LeaveOperation(VamClusterId{255})),
        "cluster identifier 255 is accepted");
}

void TestJoinTimeBoundaries()
{
    VamClusterOperation operation{};
    operation.clusterJoinInfo = VamClusterJoinInfo{
        VamClusterId{0},
        VamClusterOperationTime{
            std::chrono::milliseconds{250}}
    };

    Check(Validate(operation),
          "minimum available join time of 250 ms is accepted");

    operation.clusterJoinInfo->joinTime.duration =
        std::chrono::milliseconds{63500};

    Check(Validate(operation),
          "maximum available join time of 63500 ms is accepted");

    operation.clusterJoinInfo->joinTime.duration.reset();

    Check(Validate(operation),
          "unavailable join time is accepted");
}

void TestLeaveReasons()
{
    constexpr std::array<VamClusterLeaveReason, 9> reasons{
        VamClusterLeaveReason::NotProvided,
        VamClusterLeaveReason::ClusterLeaderLost,
        VamClusterLeaveReason::ClusterDisbandedByLeader,
        VamClusterLeaveReason::OutOfClusterBoundingBox,
        VamClusterLeaveReason::OutOfClusterSpeedRange,
        VamClusterLeaveReason::JoiningAnotherCluster,
        VamClusterLeaveReason::CancelledJoin,
        VamClusterLeaveReason::FailedJoin,
        VamClusterLeaveReason::SafetyCondition
    };

    bool allDefinedReasonsAccepted = true;
    for (const auto reason : reasons)
    {
        allDefinedReasonsAccepted =
            Validate(LeaveOperation(VamClusterId{42}, reason)) &&
            allDefinedReasonsAccepted;
    }

    Check(allDefinedReasonsAccepted,
          "all nine defined cluster leave reasons are accepted");

    Check(
        !Validate(LeaveOperation(
            VamClusterId{42},
            static_cast<VamClusterLeaveReason>(9))),
        "undefined cluster leave reason 9 is rejected");

    Check(
        !Validate(LeaveOperation(
            VamClusterId{42},
            static_cast<VamClusterLeaveReason>(255))),
        "undefined cluster leave reason 255 is rejected");
}

void TestBreakupReasons()
{
    constexpr std::array<VamClusterBreakupReason, 6> reasons{
        VamClusterBreakupReason::NotProvided,
        VamClusterBreakupReason::ClusteringPurposeCompleted,
        VamClusterBreakupReason::LeaderMovedOutOfClusterBoundingBox,
        VamClusterBreakupReason::JoiningAnotherCluster,
        VamClusterBreakupReason::EnteringLowRiskAreaBasedOnMaps,
        VamClusterBreakupReason::ReceptionOfCpmContainingCluster
    };

    bool allDefinedReasonsAccepted = true;
    for (const auto reason : reasons)
    {
        VamClusterOperation operation{};
        operation.clusterBreakupInfo = VamClusterBreakupInfo{
            reason,
            VamClusterOperationTime{
                std::chrono::milliseconds{250}}
        };

        allDefinedReasonsAccepted =
            Validate(operation) && allDefinedReasonsAccepted;
    }

    Check(allDefinedReasonsAccepted,
          "all six defined cluster breakup reasons are accepted");

    VamClusterOperation invalid{};
    invalid.clusterBreakupInfo = VamClusterBreakupInfo{
        static_cast<VamClusterBreakupReason>(6),
        VamClusterOperationTime{}
    };

    Check(!Validate(invalid),
          "undefined cluster breakup reason 6 is rejected");
}

void TestInvalidTimes()
{
    constexpr std::array<std::int64_t, 5> invalidValues{
        -250,
        0,
        249,
        251,
        63750
    };

    bool allInvalidTimesRejected = true;

    for (const auto milliseconds : invalidValues)
    {
        auto idChange = LeaveOperation();
        idChange.clusterIdChangeTimeInfo =
            VamClusterOperationTime{
                std::chrono::milliseconds{milliseconds}};

        auto join = VamClusterOperation{};
        join.clusterJoinInfo = VamClusterJoinInfo{
            VamClusterId{42},
            VamClusterOperationTime{
                std::chrono::milliseconds{milliseconds}}
        };

        auto breakup = VamClusterOperation{};
        breakup.clusterBreakupInfo = VamClusterBreakupInfo{
            VamClusterBreakupReason::NotProvided,
            VamClusterOperationTime{
                std::chrono::milliseconds{milliseconds}}
        };

        allInvalidTimesRejected =
            !Validate(idChange) &&
            !Validate(join) &&
            !Validate(breakup) &&
            allInvalidTimesRejected;
    }

    Check(
        allInvalidTimesRejected,
        "negative, zero, non-quarter-second and out-of-range times are rejected");
}

void TestClusterIdChangeTimePresence()
{
    VamClusterOperation withoutChangeTime = LeaveOperation();

    Check(
        Validate(withoutChangeTime),
        "cluster ID change time field may be absent");

    VamClusterOperation unavailableChangeTime = LeaveOperation();
    unavailableChangeTime.clusterIdChangeTimeInfo =
        VamClusterOperationTime{};

    Check(
        Validate(unavailableChangeTime),
        "present cluster ID change time may be unavailable");

    VamClusterOperation availableChangeTime = LeaveOperation();
    availableChangeTime.clusterIdChangeTimeInfo =
        VamClusterOperationTime{
            std::chrono::milliseconds{500}};

    Check(
        Validate(availableChangeTime),
        "present cluster ID change time accepts 500 ms");
}

void TestIndependentOptionalFields()
{
    VamClusterOperation operation{};

    operation.clusterJoinInfo = VamClusterJoinInfo{
        VamClusterId{0},
        VamClusterOperationTime{}
    };

    operation.clusterLeaveInfo = VamClusterLeaveInfo{
        VamClusterId{255},
        VamClusterLeaveReason::CancelledJoin
    };

    operation.clusterBreakupInfo = VamClusterBreakupInfo{
        VamClusterBreakupReason::ClusteringPurposeCompleted,
        VamClusterOperationTime{
            std::chrono::milliseconds{1000}}
    };

    operation.clusterIdChangeTimeInfo = VamClusterOperationTime{};

    Check(
        Validate(operation),
        "multiple independent operation fields may coexist");
}

} // namespace

int main()
{
    TestEmptyOperation();
    TestIdentifierBoundaries();
    TestJoinTimeBoundaries();
    TestLeaveReasons();
    TestBreakupReasons();
    TestInvalidTimes();
    TestClusterIdChangeTimePresence();
    TestIndependentOptionalFields();

    if (failures != 0)
    {
        std::cout << "VAM_CLUSTER_OPERATION_VALIDATION=FAIL failures="
                  << failures << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "VAM_CLUSTER_OPERATION_VALIDATION=PASS\n";
    return EXIT_SUCCESS;
}
