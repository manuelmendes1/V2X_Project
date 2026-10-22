#include "vanetza_integration/cpm/perception/cpm_track_history.h"

namespace
{

constexpr std::int64_t kMaxTimestampItsMs = 4398046511103LL;

bool IsValidTimestamp(std::int64_t timestampItsMs)
{
    return timestampItsMs >= 0 &&
           timestampItsMs <= kMaxTimestampItsMs;
}

bool Reject(std::string& error, const char* reason)
{
    error = reason;
    return false;
}

} // namespace

bool
CpmTrackHistory::Observe(
    std::uint16_t objectId,
    std::int64_t observationTimeItsMs,
    std::int64_t& outputFirstObservedTimeItsMs,
    std::string& error)
{
    if (objectId == 0)
    {
        return Reject(error, "Track object ID must be non-zero");
    }

    if (!IsValidTimestamp(observationTimeItsMs))
    {
        return Reject(
            error,
            "Observation timestamp is outside the TimestampIts range");
    }

    const auto found = m_tracks.find(objectId);

    if (found == m_tracks.end())
    {
        const Track newTrack{
            observationTimeItsMs,
            observationTimeItsMs};

        const auto inserted = m_tracks.emplace(objectId, newTrack);

        outputFirstObservedTimeItsMs =
            inserted.first->second.firstObservedTimeItsMs;

        error.clear();
        return true;
    }

    // Aceitar timestamps iguais permite observações repetidas no mesmo
    // instante; apenas uma regressão estrita viola a ordem temporal.
    if (observationTimeItsMs < found->second.lastObservedTimeItsMs)
    {
        return Reject(
            error,
            "Observation timestamp regresses for this track");
    }

    // Atualizar apenas depois de validar o timestamp.
    found->second.lastObservedTimeItsMs = observationTimeItsMs;
    outputFirstObservedTimeItsMs =
        found->second.firstObservedTimeItsMs;

    error.clear();
    return true;
}

bool
CpmTrackHistory::Forget(
    std::uint16_t objectId,
    std::string& error)
{
    if (objectId == 0)
    {
        return Reject(error, "Track object ID must be non-zero");
    }

    const auto found = m_tracks.find(objectId);

    if (found == m_tracks.end())
    {
        return Reject(error, "Track history does not exist");
    }

    m_tracks.erase(found);
    error.clear();
    return true;
}
