#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace ns3
{

enum class DenmTriggerSource : uint8_t
{
    SCENARIO = 0,
    SUMO,
    APPLICATION,
    TEST,
    MANUAL
};

enum class DenmTriggerEventType : uint8_t
{
    HAZARD_DETECTED = 0,
    HAZARD_UPDATED,
    HAZARD_CLEARED
};

struct DenmTriggerRequest
{
    DenmTriggerSource source = DenmTriggerSource::TEST;
    DenmTriggerEventType eventType = DenmTriggerEventType::HAZARD_DETECTED;

    // Application-level originator and event identity.
    // These are independent from ActionID and ns-3 packet metadata.
    std::string vehicleId;
    std::string eventId;

    double x = 0.0;
    double y = 0.0;

    uint32_t causeCode = 0;
    double relevanceDistanceMeters = 0.0;
    uint64_t validityDurationMs = 0;
    uint64_t triggerTimestampUs = 0;

    std::string reason;
};

class DenmTrigger
{
  public:
    using TriggerCallback = std::function<void(const DenmTriggerRequest&)>;

    explicit DenmTrigger(TriggerCallback callback = {});

    bool Submit(const DenmTriggerRequest& request);

  private:
    static bool Validate(const DenmTriggerRequest& request, std::string& error);

    TriggerCallback m_callback;
};

} // namespace ns3
