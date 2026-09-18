#include "rx_context_builder.h"

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"

#include <ns3/simulator.h>

using namespace ns3;

PacketContext RxContextBuilder::Build(uint16_t destinationPort, const VehicleState& receiver) {

    PacketContext ctx;

    // ------------------------------------------------------------------
    // Simulation
    // ------------------------------------------------------------------

    const auto now = Simulator::Now();

    ctx.simulationStep = SimulationContext::simulationStep;
    ctx.simulationTime = now.GetSeconds();

    // Timestamp temporal imutável do PacketContext RX.
    // A unidade é microssegundos do relógio de simulação ns-3.
    ctx.timestamp = now.GetMicroSeconds();

    // ------------------------------------------------------------------
    // Receiver
    // ------------------------------------------------------------------

    // receiverNodeId is routing/input information used by RxContext to
    // resolve the authoritative receiver identity. The remaining receiver
    // identity fields are established and validated exclusively by RxContext.
    ctx.receiverNodeId = receiver.identity.nodeId;

    // ------------------------------------------------------------------
    // Informação disponível proveniente do BTP
    // ------------------------------------------------------------------

    ctx.btpDestinationPort = destinationPort;
    ctx.reason = "BTP dst_port=" + std::to_string(destinationPort);

    return ctx;
}
