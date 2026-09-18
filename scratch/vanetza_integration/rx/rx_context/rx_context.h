#pragma once

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/context/vehicle_state.h"


struct CamMessage;

class RxContext
{
public:

    //=========================================================================
    // Resolve sender/receiver from GeoNetworking (official RX path)
    //=========================================================================
    static bool Resolve(
        ns3::PacketContext& ctx,
        ns3::VehicleState*& sender,
        ns3::VehicleState*& receiver);

    //=========================================================================
    // Compatibility path (legacy CAM-based resolution)
    //=========================================================================
    static bool Resolve(
        ns3::PacketContext& ctx,
        const CamMessage& message,
        ns3::VehicleState*& sender,
        ns3::VehicleState*& receiver);

private:

    //=========================================================================
    // Internal helpers
    //=========================================================================


    static ns3::VehicleState* ResolveSender(
        ns3::PacketContext& ctx,
        uint32_t stationId);

    static ns3::VehicleState* ResolveSender(
        ns3::PacketContext& ctx);


    static ns3::VehicleState* ResolveReceiver(
        ns3::PacketContext& ctx);

    static bool PopulateSenderContext(
        ns3::PacketContext& ctx,
        const ns3::VehicleState& sender);

    static bool PopulateReceiverContext(
        ns3::PacketContext& ctx,
        const ns3::VehicleState& receiver);
};
