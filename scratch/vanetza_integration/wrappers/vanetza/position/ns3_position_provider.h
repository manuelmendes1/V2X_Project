#pragma once

#include <vanetza/common/position_provider.hpp>

namespace ns3 {

    class VehicleState;

    class Ns3PositionProvider : public vanetza::PositionProvider {

        public:
            explicit Ns3PositionProvider(VehicleState& vehicle);
            const vanetza::PositionFix& position_fix() override;   // <-- const&

        private:
            VehicleState& m_vehicle;
            vanetza::PositionFix m_fix;   // <-- membro para poder retornar referência
    };

} // namespace ns3
