#include "ns3_channel_load_probe.h"

#include "ns3/wifi-phy.h"
#include "ns3/wifi-phy-listener.h"

#include <algorithm>
#include <cstdint>

namespace ns3
{

class Ns3ChannelLoadProbe::Listener final : public WifiPhyListener
{
public:
    explicit Listener(
        Ns3ChannelLoadProbe& owner)
        : m_owner(owner)
    {
    }

    void NotifyRxStart(
        Time duration) override
    {
        ++m_owner.m_rxStartCount;

        m_owner.AddBusyInterval(
            Simulator::Now(),
            duration);
    }

    void NotifyRxEndOk() override
    {
    }

    void NotifyRxEndError(
        const WifiTxVector&) override
    {
    }

    void NotifyTxStart(
        Time duration,
        dBm_u) override
    {
        ++m_owner.m_txStartCount;

        m_owner.AddBusyInterval(
            Simulator::Now(),
            duration);
    }

    void NotifyCcaBusyStart(
        Time duration,
        WifiChannelListType channelType,
        const std::vector<Time>&) override
    {
        if (channelType !=
            WIFI_CHANLIST_PRIMARY)
        {
            return;
        }

        ++m_owner.m_ccaBusyStartCount;

        m_owner.AddBusyInterval(
            Simulator::Now(),
            duration);
    }

    void NotifySwitchingStart(
        Time) override
    {
    }

    void NotifySleep() override
    {
    }

    void NotifyOff() override
    {
    }

    void NotifyWakeup() override
    {
    }

    void NotifyOn() override
    {
    }

private:
    Ns3ChannelLoadProbe& m_owner;
};

Ns3ChannelLoadProbe::Ns3ChannelLoadProbe(
    Ptr<WifiPhy> phy,
    Time measurementWindow)
    : m_phy(phy),
      m_measurementWindow(measurementWindow)
{
    NS_ABORT_MSG_IF(
        m_phy == nullptr,
        "[DCC] Cannot create ChannelLoadProbe: WifiPhy is null");

    NS_ABORT_MSG_IF(
        !m_measurementWindow.IsStrictlyPositive(),
        "[DCC] ChannelLoadProbe window must be > 0");

    m_listener =
        std::make_shared<Listener>(*this);

    m_phy->RegisterListener(
        m_listener);
}

Ns3ChannelLoadProbe::~Ns3ChannelLoadProbe()
{
    if (m_phy != nullptr &&
        m_listener != nullptr &&
        m_phy->GetState() != nullptr)
    {
        m_phy->UnregisterListener(
            m_listener);
    }
}

void
Ns3ChannelLoadProbe::AddBusyInterval(
    Time start,
    Time duration)
{
    if (!duration.IsStrictlyPositive())
    {
        return;
    }

    m_intervals.push_back(
        Interval{
            start,
            start + duration});
}

void
Ns3ChannelLoadProbe::Prune(
    Time windowStart)
{
    m_intervals.erase(
        std::remove_if(
            m_intervals.begin(),
            m_intervals.end(),
            [windowStart](
                const Interval& interval)
            {
                return interval.end <=
                       windowStart;
            }),
        m_intervals.end());
}

Time
Ns3ChannelLoadProbe::ComputeBusy(
    Time windowStart,
    Time windowEnd) const
{
    if (windowEnd <= windowStart)
    {
        return Seconds(0);
    }

    std::vector<Interval> clipped;

    clipped.reserve(
        m_intervals.size());

    for (const auto& interval :
         m_intervals)
    {
        const Time start =
            std::max(
                interval.start,
                windowStart);

        const Time end =
            std::min(
                interval.end,
                windowEnd);

        if (end > start)
        {
            clipped.push_back(
                Interval{
                    start,
                    end});
        }
    }

    if (clipped.empty())
    {
        return Seconds(0);
    }

    std::sort(
        clipped.begin(),
        clipped.end(),
        [](const Interval& a,
           const Interval& b)
        {
            return a.start < b.start;
        });

    Time busy =
        Seconds(0);

    Time currentStart =
        clipped.front().start;

    Time currentEnd =
        clipped.front().end;

    for (std::size_t i = 1;
         i < clipped.size();
         ++i)
    {
        if (clipped[i].start <=
            currentEnd)
        {
            currentEnd =
                std::max(
                    currentEnd,
                    clipped[i].end);
        }
        else
        {
            busy +=
                currentEnd -
                currentStart;

            currentStart =
                clipped[i].start;

            currentEnd =
                clipped[i].end;
        }
    }

    busy +=
        currentEnd -
        currentStart;

    return busy;
}

vanetza::dcc::ChannelLoad
Ns3ChannelLoadProbe::Measure()
{
    const Time now =
        Simulator::Now();

    const Time windowStart =
        now -
        m_measurementWindow;

    Prune(windowStart);

    const Time busy =
        ComputeBusy(
            windowStart,
            now);

    /*
     * Snapshot dos eventos observados
     * desde a última medição.
     */
    m_lastRxStartCount =
        m_rxStartCount;

    m_lastTxStartCount =
        m_txStartCount;

    m_lastCcaBusyStartCount =
        m_ccaBusyStartCount;

    m_lastBusyTime =
        busy;

    /*
     * Reinicia os contadores para que a
     * próxima medição represente uma
     * nova janela de observação.
     */
    m_rxStartCount = 0;
    m_txStartCount = 0;
    m_ccaBusyStartCount = 0;

    const double busyUs =
        busy.GetMicroSeconds();

    const double windowUs =
        m_measurementWindow
            .GetMicroSeconds();

    const double cbr =
        windowUs > 0.0
            ? busyUs / windowUs
            : 0.0;

    m_lastValue =
        std::clamp(
            cbr,
            0.0,
            1.0);

    return vanetza::dcc::ChannelLoad(
        m_lastValue);
}

double
Ns3ChannelLoadProbe::Value() const
{
    return m_lastValue;
}

uint64_t
Ns3ChannelLoadProbe::RxStartCount() const
{
    return m_lastRxStartCount;
}

uint64_t
Ns3ChannelLoadProbe::TxStartCount() const
{
    return m_lastTxStartCount;
}

uint64_t
Ns3ChannelLoadProbe::CcaBusyStartCount() const
{
    return m_lastCcaBusyStartCount;
}

Time
Ns3ChannelLoadProbe::BusyTime() const
{
    return m_lastBusyTime;
}

} // namespace ns3
