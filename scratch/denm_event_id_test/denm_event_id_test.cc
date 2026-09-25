#include "vanetza_integration/denm/types/denm_event_id.h"

#include <iostream>
#include <unordered_set>

using namespace ns3;

namespace
{

bool
Expect(bool condition, const char* description)
{
    if (condition)
    {
        std::cout << "PASS " << description << "\n";
        return true;
    }

    std::cout << "FAIL " << description << "\n";
    return false;
}

} // namespace

int
main()
{
    bool ok = true;

    std::cout << "===== DENM EVENT ID TEST =====\n";

    DenmEventId eventA{
        "veh0",
        "event-A"
    };

    DenmEventId eventA2{
        "veh0",
        "event-A"
    };

    DenmEventId eventB{
        "veh0",
        "event-B"
    };

    DenmEventId eventOtherVehicle{
        "veh1",
        "event-A"
    };

    ok &= Expect(
        eventA == eventA2,
        "identical vehicle/event identities compare equal");

    ok &= Expect(
        eventA != eventB,
        "different event identities compare different");

    ok &= Expect(
        eventA != eventOtherVehicle,
        "different vehicle identities compare different");

    ok &= Expect(
        eventA.vehicleId == "veh0" &&
        eventA.eventId == "event-A",
        "identity fields preserved");

    std::unordered_set<DenmEventId, DenmEventIdHash> identities;

    identities.insert(eventA);
    identities.insert(eventA2);
    identities.insert(eventB);
    identities.insert(eventOtherVehicle);

    ok &= Expect(
        identities.size() == 3,
        "equal identities collapse to one hash-set entry");

    if (!ok)
    {
        std::cout << "DENM_EVENT_ID_TEST_EXIT=1\n";
        return 1;
    }

    std::cout << "DENM_EVENT_ID_TEST_EXIT=0\n";
    return 0;
}
