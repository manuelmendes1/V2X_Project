#include "vanetza_integration/cpm/transport/cpm_transport_policy.h"

#include <cstdint>
#include <iostream>
#include <optional>

namespace
{

bool Check(bool condition, const char* name)
{
    std::cout
        << (condition ? "PASS " : "FAIL ")
        << name << '\n';
    return condition;
}

} // namespace

int main()
{
    bool passed = true;

    // This is a test fixture only, not a normative traffic-class selection.
    constexpr std::uint8_t testTrafficClass = 2;

    const ns3::BtpTxRequest request =
        ns3::MakeCpmBtpRequest(testTrafficClass);

    passed &= Check(
        request.destinationPort == 2009,
        "CPM_BTP_PORT_2009");

    passed &= Check(
        request.destinationPortInfo == 0,
        "CPM_BTP_PORT_INFO_EXPLICIT");

    passed &= Check(
        request.itsAid == 639,
        "CPM_ITS_AID_639");

    passed &= Check(
        request.transport == ns3::BtpTransportType::SHB,
        "CPM_GEONET_TRANSPORT_SHB");

    passed &= Check(
        request.communicationProfile ==
            ns3::BtpCommunicationProfile::ITS_G5,
        "CPM_COMMUNICATION_PROFILE_ITS_G5");

    passed &= Check(
        request.trafficClass == testTrafficClass,
        "CPM_TRAFFIC_CLASS_PASSED_BY_CALLER");

    passed &= Check(
        !request.destinationArea.has_value(),
        "CPM_SHB_HAS_NO_DESTINATION_AREA");

    passed &= Check(
        request.maximumLifetimeMs.has_value() &&
            *request.maximumLifetimeMs == 1000,
        "CPM_MAXIMUM_LIFETIME_1000_MS");

    passed &= Check(
        !request.maximumHopLimit.has_value(),
        "CPM_HOP_LIMIT_LEFT_TO_MIB");

    if (passed)
    {
        std::cout << "CPM_TRANSPORT_POLICY_TESTS=PASS\n";
        return 0;
    }

    std::cout << "CPM_TRANSPORT_POLICY_TESTS=FAIL\n";
    return 1;
}
