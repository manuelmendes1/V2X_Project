#pragma once

#include <vanetza/common/position_provider.hpp>

namespace ns3
{

class CpmRsuPositionProvider final
    : public vanetza::PositionProvider
{
public:
    CpmRsuPositionProvider(double latitudeDeg, double longitudeDeg);
    const vanetza::PositionFix& position_fix() override;

private:
    double m_latitudeDeg;
    double m_longitudeDeg;
    vanetza::PositionFix m_fix;
};

} // namespace ns3
