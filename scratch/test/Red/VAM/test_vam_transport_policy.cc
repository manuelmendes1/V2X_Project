#include "vanetza_integration/vam/transport/vam_transport_policy.h"

#include <cstdint>
#include <iostream>

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
    constexpr std::uint8_t testTrafficClass = 3;

    const BtpTxRequest request =
        MakeVamBtpRequest(testTrafficClass);

    passed &= Check(
        request.destinationPort == 2018,
        "VAM_BTP_PORT_2018");

    passed &= Check(
        request.destinationPortInfo == 0,
        "VAM_BTP_PORT_INFO_EXPLICIT");

    passed &= Check(
        request.itsAid == 638,
        "VAM_ITS_AID_638");

    passed &= Check(
        request.transport == BtpTransportType::SHB,
        "VAM_GEONET_TRANSPORT_SHB");

    passed &= Check(
        request.communicationProfile ==
            BtpCommunicationProfile::ITS_G5,
        "VAM_COMMUNICATION_PROFILE_ITS_G5");

    passed &= Check(
        request.trafficClass == testTrafficClass,
        "VAM_TRAFFIC_CLASS_PASSED_BY_CALLER");

    passed &= Check(
        !request.destinationArea.has_value(),
        "VAM_SHB_HAS_NO_DESTINATION_AREA");

    passed &= Check(
        request.maximumLifetimeMs.has_value() &&
            *request.maximumLifetimeMs == 1000,
        "VAM_MAXIMUM_LIFETIME_1000_MS");

    passed &= Check(
        !request.maximumHopLimit.has_value(),
        "VAM_HOP_LIMIT_LEFT_TO_MIB");

    if (!passed)
    {
        std::cout << "VAM_TRANSPORT_POLICY_TESTS=FAIL\n";
        return 1;
    }

    std::cout << "VAM_TRANSPORT_POLICY_TESTS=PASS\n";
    return 0;
}
