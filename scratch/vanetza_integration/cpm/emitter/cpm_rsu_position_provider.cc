#include "vanetza_integration/cpm/emitter/cpm_rsu_position_provider.h"

#include <chrono>

#include <ns3/simulator.h>

#include <vanetza/common/clock.hpp>
#include <vanetza/units/angle.hpp>

namespace ns3
{

CpmRsuPositionProvider::CpmRsuPositionProvider(
    double latitudeDeg,
    double longitudeDeg)
    : m_latitudeDeg(latitudeDeg),
      m_longitudeDeg(longitudeDeg)
{
}

const vanetza::PositionFix&
CpmRsuPositionProvider::position_fix()
{
    const auto nowUs =
        static_cast<vanetza::Clock::rep>(
            Simulator::Now().GetMicroSeconds());

    m_fix.timestamp =
        vanetza::Clock::time_point{
            std::chrono::microseconds(nowUs)};

    m_fix.latitude =
        vanetza::units::GeoAngle::from_value(m_latitudeDeg);

    m_fix.longitude =
        vanetza::units::GeoAngle::from_value(m_longitudeDeg);

    m_fix.confidence = vanetza::PositionConfidence();

    return m_fix;
}

} // namespace ns3
