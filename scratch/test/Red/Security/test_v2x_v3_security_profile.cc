#include "vanetza_integration/wrappers/vanetza/security/v2x_v3_security_profile.h"

#include <cstdint>
#include <iostream>

namespace
{
bool Check(
    std::uint32_t aid,
    ns3::V2xV3SecurityProfile expected,
    const char* label)
{
    const auto actual = ns3::ClassifyV2xV3SecurityProfile(aid);
    if (actual != expected)
    {
        std::cerr << "[FAIL] " << label << " aid=" << aid << '\n';
        return false;
    }
    std::cout << "[PASS] " << label << '\n';
    return true;
}
}

int main()
{
    using ns3::V2xV3SecurityProfile;
    bool ok = true;
    ok &= Check(36, V2xV3SecurityProfile::VehicleCamDenm, "CAM AID routes to vehicle V3");
    ok &= Check(37, V2xV3SecurityProfile::VehicleCamDenm, "DENM AID routes to vehicle V3");
    ok &= Check(137, V2xV3SecurityProfile::SpatemRsu, "SPATEM AID routes to SPATEM V3");
    ok &= Check(639, V2xV3SecurityProfile::CpmRsu, "CPM AID routes to CPM V3");
    ok &= Check(0, V2xV3SecurityProfile::Unsupported, "AID zero is unsupported");
    ok &= Check(38, V2xV3SecurityProfile::Unsupported, "unconfigured AID is unsupported");
    ok &= Check(638, V2xV3SecurityProfile::Unsupported, "VAM remains unsupported");
    ok &= Check(640, V2xV3SecurityProfile::Unsupported, "unknown high AID is unsupported");
    return ok ? 0 : 1;
}
