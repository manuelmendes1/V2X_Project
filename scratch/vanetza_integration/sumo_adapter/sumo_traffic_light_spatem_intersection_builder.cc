#include "sumo_traffic_light_spatem_intersection_builder.h"

#include <utility>

#include "sumo_traffic_light_spatem_mapper.h"

namespace ns3
{

SumoTrafficLightSpatemIntersectionBuilder::
    SumoTrafficLightSpatemIntersectionBuilder(
        const SumoSpatemIntersectionConfig& config)
    : m_config(config)
{
    m_intersection.intersectionId =
        m_config.intersectionId;

    m_intersection.revision =
        m_config.revision;

    m_intersection.statusBits =
        m_config.statusBits;

    m_intersection.region =
        m_config.region;
}

bool
SumoTrafficLightSpatemIntersectionBuilder::Update(
    const SumoTrafficLightState& input,
    const SumoSpatemTimeReference& reference,
    std::string& error)
{
    if (m_config.trafficLightId.empty())
    {
        error =
            "SPATEM intersection builder requires "
            "an explicit trafficLightId";
        return false;
    }

    if (m_config.statusBits & 0xC000U)
    {
        error =
            "SPATEM statusBits contain reserved bits";
        return false;
    }

    if (input.trafficLightId !=
        m_config.trafficLightId)
    {
        error =
            "SUMO trafficLightId does not match "
            "the configured SPATEM intersection";
        return false;
    }

    if (input.linkIndex > 255U)
    {
        error =
            "SUMO linkIndex cannot be represented "
            "as SPATEM signalGroup";
        return false;
    }

    SpatemMovement movement;
    if (!SumoTrafficLightSpatemMapper::Map(
            input,
            reference,
            movement,
            error))
    {
        return false;
    }

    if (movement.signalGroup !=
        static_cast<uint8_t>(input.linkIndex))
    {
        error =
            "SUMO/SPATEM signalGroup mapping is inconsistent";
        return false;
    }

    // Replace the state for this controlled link.
    // A new observation of the same link must not append
    // another movement entry.
    m_movements[movement.signalGroup] =
        std::move(movement);

    SpatemIntersection updated;
    updated.region =
        m_config.region;

    updated.intersectionId =
        m_config.intersectionId;

    updated.revision =
        m_config.revision;

    updated.statusBits =
        m_config.statusBits;

    updated.minuteOfYear =
        reference.minuteOfYear;

    updated.dSecond =
        reference.dSecond;

    for (const auto& entry : m_movements)
    {
        updated.movements.push_back(entry.second);
    }

    m_intersection =
        std::move(updated);

    error.clear();
    return true;
}

const SpatemIntersection&
SumoTrafficLightSpatemIntersectionBuilder::GetIntersection() const
{
    return m_intersection;
}

} // namespace ns3
