#include "vanetza_integration/context/sumo_person_vru_subject_synchronizer/sumo_person_vru_subject_synchronizer.h"

#include "vanetza_integration/vru/profile/sumo_vru_profile_mapper.h"

namespace ns3
{

bool
SumoVruSubjectSynchronizer::Apply(
    const SumoPersonDatagram& datagram,
    SumoPersonRegistry& persons,
    SumoVruSubjectRegistry& subjects)
{
    if (!persons.Apply(datagram))
    {
        return false;
    }

    return SynchronizeAccepted(datagram, subjects);
}

bool
SumoVruSubjectSynchronizer::SynchronizeAccepted(
    const SumoPersonDatagram& datagram,
    SumoVruSubjectRegistry& subjects)
{
    if (datagram.personId.empty())
    {
        return false;
    }

    switch (datagram.eventType)
    {
    case SumoPersonEventType::Upsert:
    {
        if (!datagram.sample.has_value())
        {
            return false;
        }

        const auto profile =
            vanetza_integration::vru::SumoVruProfileMapper::Map(
                datagram.sumoVClass);

        return subjects.Upsert(datagram.personId, profile);
    }

    case SumoPersonEventType::Remove:
        if (datagram.sample.has_value())
        {
            return false;
        }

        // An accepted source REMOVE is successful even if the derived
        // subject is already absent. This keeps removal idempotent.
        subjects.Remove(datagram.personId);
        return true;

    default:
        return false;
    }
}

} // namespace ns3
