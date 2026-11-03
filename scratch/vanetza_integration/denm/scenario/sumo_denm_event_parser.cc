#include "sumo_denm_event_parser.h"

#include <sstream>
#include <stdexcept>
#include <string>

namespace ns3
{

std::optional<DenmScenarioEvent>
SumoDenmEventParser::Parse(const std::string& message)
{
    std::stringstream ss(message);

    std::string prefix;
    std::string vehicleId;
    std::string eventId;
    std::string eventType;
    std::string token;
    std::string reason;

    if (!std::getline(ss, prefix, ','))
    {
        return std::nullopt;
    }

    const bool isSumoEvent =
        prefix == "DENM_EVENT";

    const bool isScenarioEvent =
        prefix == "DENM_SCENARIO_EVENT";

    if (!isSumoEvent && !isScenarioEvent)
    {
        return std::nullopt;
    }

    if (!std::getline(ss, vehicleId, ',') ||
        !std::getline(ss, eventId, ',') ||
        !std::getline(ss, eventType, ',') ||
        !std::getline(ss, token, ','))
    {
        return std::nullopt;
    }

    DenmScenarioEvent event;

    event.source = isSumoEvent
        ? DenmTriggerSource::SUMO
        : DenmTriggerSource::SCENARIO;

    event.vehicleId = vehicleId;
    event.eventId = eventId;

    if (eventType == "DETECTED")
    {
        event.eventType =
            DenmTriggerEventType::HAZARD_DETECTED;
    }
    else if (eventType == "UPDATED")
    {
        event.eventType =
            DenmTriggerEventType::HAZARD_UPDATED;
    }
    else if (eventType == "CLEARED")
    {
        event.eventType =
            DenmTriggerEventType::HAZARD_CLEARED;
    }
    else
    {
        return std::nullopt;
    }

    try
    {
        event.x = std::stod(token);

        if (!std::getline(ss, token, ','))
        {
            return std::nullopt;
        }
        event.y = std::stod(token);

        if (!std::getline(ss, token, ','))
        {
            return std::nullopt;
        }
        event.causeCode =
            static_cast<uint32_t>(std::stoul(token));

        if (!std::getline(ss, token, ','))
        {
            return std::nullopt;
        }
        event.relevanceDistanceMeters =
            std::stod(token);

        if (!std::getline(ss, token, ','))
        {
            return std::nullopt;
        }
        event.validityDurationMs =
            static_cast<uint64_t>(std::stoull(token));

        if (!std::getline(ss, token, ','))
        {
            return std::nullopt;
        }
        event.triggerTimeUs =
            static_cast<uint64_t>(std::stoull(token));

        if (!std::getline(ss, reason))
        {
            return std::nullopt;
        }

        event.reason = reason;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }

    if (event.vehicleId.empty() ||
        event.eventId.empty() ||
        event.triggerTimeUs == 0 ||
        event.causeCode == 0)
    {
        return std::nullopt;
    }

    return event;
}

} // namespace ns3
