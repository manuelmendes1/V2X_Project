#pragma once

#include <cstdint>

namespace ns3
{

class DenmRepetitionPolicy
{
public:
    void Start(
        uint64_t firstTransmissionTimeUs,
        uint64_t intervalUs,
        uint32_t maxRepetitions);

    bool ShouldRepeat(
        uint64_t nowUs) const;

    void Consume(
        uint64_t nowUs);

    void Reset();

    uint32_t RepetitionCount() const;

    uint64_t NextRepetitionTimeUs() const;

private:
    uint64_t m_intervalUs = 0;
    uint64_t m_nextRepetitionTimeUs = 0;
    uint32_t m_repetitionCount = 0;
    uint32_t m_maxRepetitions = 0;
    bool m_active = false;
};

} // namespace ns3
