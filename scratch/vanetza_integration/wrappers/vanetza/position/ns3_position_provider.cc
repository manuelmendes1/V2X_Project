#include "ns3_position_provider.h"
#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"

#include <vanetza/units/angle.hpp>

#include "ns3/simulator.h"

#include <chrono>
#include <stdexcept>

namespace ns3 {

    Ns3PositionProvider::Ns3PositionProvider(VehicleState& vehicle) : m_vehicle(vehicle) {}

    const vanetza::PositionFix& Ns3PositionProvider::position_fix() {

        // Vanetza::Clock usa a época 2004-01-01.
        // Simulator::Now() já representa o tempo decorrido da simulação,
        // portanto deve ser convertido diretamente para a duração do Clock.
        const auto now_us = static_cast<vanetza::Clock::rep>(Simulator::Now().GetMicroSeconds());

        m_fix.timestamp = vanetza::Clock::time_point{std::chrono::microseconds(now_us)};

        std::optional<Wgs84Position> suppliedGeodetic;
        if (m_vehicle.mobility.geodeticPosition.has_value())
        {
            const auto& sample = *m_vehicle.mobility.geodeticPosition;
            suppliedGeodetic = Wgs84Position{
                sample.latitudeDeg,
                sample.longitudeDeg
            };
        }

        Wgs84Position geodetic{};
        std::string error;
        if (!SumoGeodeticTransform::ResolveWgs84(
                suppliedGeodetic,
                SimulationContext::sumoProjection,
                SumoProjectedPosition{
                    m_vehicle.mobility.position.x,
                    m_vehicle.mobility.position.y
                },
                geodetic,
                error))
        {
            throw std::runtime_error(
                "Cannot provide GeoNetworking position without valid WGS84: " +
                error);
        }

        m_fix.latitude =
            vanetza::units::GeoAngle::from_value(geodetic.latitudeDeg);

        m_fix.longitude =
            vanetza::units::GeoAngle::from_value(geodetic.longitudeDeg);

        // Altitude e confiança — valores por omissão
        m_fix.confidence = vanetza::PositionConfidence();

        return m_fix;
    }

} // namespace ns3
