#pragma once

namespace ns3
{

class SimulationExport
{
public:

    // ============================================================
    // Exporta o estado atual dos veículos do ns-3 para o SUMO.
    // ============================================================

    static void SendVehiclesToSumo();
};

} // namespace ns3
