#include "vanetza_integration/context/sumo_person_registry/sumo_person_registry.h"

#include <cmath>

namespace ns3
{
namespace
{

constexpr double kTraCIUnavailable = -1073741824.0;

bool IsValidCoordinateOrAngle(double value)
{
    return std::isfinite(value) && value != kTraCIUnavailable;
}

bool IsValidSample(const SumoPersonSample& sample)
{
    return IsValidCoordinateOrAngle(sample.x) &&
           IsValidCoordinateOrAngle(sample.y) &&
           std::isfinite(sample.speed) &&
           sample.speed >= 0.0 &&
           sample.speed != kTraCIUnavailable &&
           IsValidCoordinateOrAngle(sample.angleDeg);
}

} // namespace

bool
SumoPersonRegistry::Apply(const SumoPersonDatagram& datagram)
{
    if (datagram.personId.empty())
    {
        return false;
    }

    switch (datagram.eventType)
    {
    case SumoPersonEventType::Upsert:
        if (!datagram.sample.has_value() ||
            !IsValidSample(*datagram.sample))
        {
            return false;
        }
        break;

    case SumoPersonEventType::Remove:
        if (datagram.sample.has_value())
        {
            return false;
        }
        break;

    default:
        return false;
    }

    const auto lastEvent = m_lastEventTimeUs.find(datagram.personId);
    if (lastEvent != m_lastEventTimeUs.end() &&
        datagram.simulationTimeUs <= lastEvent->second)
    {
        return false;
    }

    switch (datagram.eventType)
    {
    case SumoPersonEventType::Upsert:
    {
        // Preserve the first-observation timestamp while the person remains
        // active. After an accepted REMOVE, the active entry is absent and
        // the next accepted UPSERT starts a new observation period.
        const auto existingPerson = m_persons.find(datagram.personId);

        const std::uint64_t firstObservedAtUs =
            existingPerson == m_persons.end()
                ? datagram.simulationTimeUs
                : existingPerson->second.firstObservedAtUs;

        m_persons.insert_or_assign(
            datagram.personId,
            SumoPersonState{
                *datagram.sample,
                firstObservedAtUs,
                datagram.simulationTimeUs});
        break;
    }

    case SumoPersonEventType::Remove:
        m_persons.erase(datagram.personId);
        break;

    default:
        // The event type was validated above; retain a defensive guard.
        return false;
    }

    m_lastEventTimeUs.insert_or_assign(
        datagram.personId, datagram.simulationTimeUs);
    return true;
}

std::optional<SumoPersonState>
SumoPersonRegistry::Find(const std::string& personId) const
{
    const auto found = m_persons.find(personId);
    if (found == m_persons.end())
    {
        return std::nullopt;
    }

    return found->second;
}

std::size_t
SumoPersonRegistry::Size() const noexcept
{
    return m_persons.size();
}

bool
SumoPersonRegistry::Empty() const noexcept
{
    return m_persons.empty();
}

void
SumoPersonRegistry::Clear() noexcept
{
    m_persons.clear();
    m_lastEventTimeUs.clear();
}

} // namespace ns3
