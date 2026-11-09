#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ns3
{

enum class SumoPersonEventType : std::uint8_t
{
    Upsert,
    Remove
};

struct SumoPersonSample
{
    double x{};
    double y{};
    double speed{};
    double angleDeg{};
};

// SUMO identity and mobility only. This type deliberately owns no ns-3 or
// ITS identity and is not a VehicleState.
struct SumoPersonDatagram
{
    SumoPersonEventType eventType = SumoPersonEventType::Upsert;
    std::string personId;
    std::optional<SumoPersonSample> sample;
    std::uint64_t simulationTimeUs{};
    std::optional<std::string> sumoVClass;
};

std::optional<SumoPersonDatagram>
ParseSumoPersonDatagram(const std::string& message);

} // namespace ns3
