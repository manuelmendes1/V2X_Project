#include "rx_reception.h"

#include "vanetza_integration/cam/reception/cam_reception.h"

bool RxReception::Process(ns3::PacketContext& ctx, const CamMessage& message, ns3::VehicleState& receiver, ns3::VehicleState& sender) {
    CamReception::Process(ctx, message, receiver, sender);
    return true;
}
