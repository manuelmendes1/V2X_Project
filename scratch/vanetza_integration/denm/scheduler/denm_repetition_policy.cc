#include "denm_repetition_policy.h"

namespace ns3
{

void
DenmRepetitionPolicy::Start(
    uint64_t firstTransmissionTimeUs,
    uint64_t intervalUs,
    uint32_t maxRepetitions)
{
    m_intervalUs = intervalUs;
    m_repetitionCount = 0;
    m_maxRepetitions = maxRepetitions;

    if (intervalUs == 0 ||
        maxRepetitions == 0)
    {
        m_nextRepetitionTimeUs = 0;
        m_active = false;
        return;
    }

    m_nextRepetitionTimeUs =
        firstTransmissionTimeUs + intervalUs;

    m_active = true;
}

bool
DenmRepetitionPolicy::ShouldRepeat(
    uint64_t nowUs) const
{
    if (!m_active)
    {
        return false;
    }

    if (m_repetitionCount >= m_maxRepetitions)
    {
        return false;
    }

    return nowUs >= m_nextRepetitionTimeUs;
}

void
DenmRepetitionPolicy::Consume(
    uint64_t nowUs)
{
    if (!ShouldRepeat(nowUs))
    {
        return;
    }

    ++m_repetitionCount;

    if (m_repetitionCount >= m_maxRepetitions)
    {
        m_nextRepetitionTimeUs = 0;
        m_active = false;
        return;
    }

    // Schedule the next repetition relative to the actual
    // transmission time observed by the scheduler.
    m_nextRepetitionTimeUs =
        nowUs + m_intervalUs;
}

void
DenmRepetitionPolicy::Reset()
{
    m_intervalUs = 0;
    m_nextRepetitionTimeUs = 0;
    m_repetitionCount = 0;
    m_maxRepetitions = 0;
    m_active = false;
}

uint32_t
DenmRepetitionPolicy::RepetitionCount() const
{
    return m_repetitionCount;
}

uint64_t
DenmRepetitionPolicy::NextRepetitionTimeUs() const
{
    return m_nextRepetitionTimeUs;
}

} // namespace ns3
