#include "ns3_position_provider.h"
#include "vanetza_integration/context/vehicle_state.h"

#include <vanetza/units/angle.hpp>

#include "ns3/simulator.h"

namespace ns3 {

    Ns3PositionProvider::Ns3PositionProvider(VehicleState& vehicle) : m_vehicle(vehicle) {}

    const vanetza::PositionFix& Ns3PositionProvider::position_fix() {

        // Vanetza::Clock usa a época 2004-01-01.
        // Simulator::Now() já representa o tempo decorrido da simulação,
        // portanto deve ser convertido diretamente para a duração do Clock.
        const auto now_us = static_cast<vanetza::Clock::rep>(Simulator::Now().GetMicroSeconds());

        m_fix.timestamp = vanetza::Clock::time_point{std::chrono::microseconds(now_us)};

        // SUMO: coordenadas cartesianas locais (x, y) em metros.
        // Conversão aproximada para graus (unidade nativa do GeoAngle).
        const double meters_per_degree = 111320.0;

        m_fix.latitude = vanetza::units::GeoAngle::from_value(m_vehicle.mobility.position.y / meters_per_degree);

        m_fix.longitude = vanetza::units::GeoAngle::from_value(m_vehicle.mobility.position.x / meters_per_degree);

        // Altitude e confiança — valores por omissão
        m_fix.confidence = vanetza::PositionConfidence();

        return m_fix;
    }

} // namespace ns3
