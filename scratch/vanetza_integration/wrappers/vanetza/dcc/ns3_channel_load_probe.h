#pragma once

#include "ns3/nstime.h"

#include <vanetza/dcc/channel_load.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace ns3
{

class WifiPhy;
class WifiPhyListener;
class WifiTxVector;

class Ns3ChannelLoadProbe final
{
public:
    explicit Ns3ChannelLoadProbe(
        Ptr<WifiPhy> phy,
        Time measurementWindow = MilliSeconds(100));

    ~Ns3ChannelLoadProbe();

    Ns3ChannelLoadProbe(const Ns3ChannelLoadProbe&) = delete;
    Ns3ChannelLoadProbe& operator=(const Ns3ChannelLoadProbe&) = delete;

    vanetza::dcc::ChannelLoad Measure();

    double Value() const;

    uint64_t RxStartCount() const;
    uint64_t TxStartCount() const;
    uint64_t CcaBusyStartCount() const;

    Time BusyTime() const;

private:
    struct Interval
    {
        Time start;
        Time end;
    };

    class Listener;

    void AddBusyInterval(
        Time start,
        Time duration);

    void Prune(
        Time windowStart);

    Time ComputeBusy(
        Time windowStart,
        Time windowEnd) const;

    Ptr<WifiPhy> m_phy;

    std::shared_ptr<Listener> m_listener;

    Time m_measurementWindow;

    std::vector<Interval> m_intervals;

    double m_lastValue = 0.0;

    uint64_t m_rxStartCount = 0;
    uint64_t m_txStartCount = 0;
    uint64_t m_ccaBusyStartCount = 0;

    uint64_t m_lastRxStartCount = 0;
    uint64_t m_lastTxStartCount = 0;
    uint64_t m_lastCcaBusyStartCount = 0;

    Time m_lastBusyTime = Seconds(0);
};

} // namespace ns3
