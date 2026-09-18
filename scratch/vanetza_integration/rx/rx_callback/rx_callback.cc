#include "vanetza_integration/rx/rx_callback/rx_callback.h"

#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"

#include "ns3/packet.h"

namespace ns3
{

void
RxCallback::Receive(
    Ptr<Socket> socket)
{
    if (socket == nullptr) {
        return;
    }

    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom(from)))
    {
        if (packet && packet->GetSize() <= 200)
        {
            NS_LOG_UNCOND(
                "[RX_SOCKET_DIAG] node="
                << socket->GetNode()->GetId()
                << " size="
                << packet->GetSize()
                << " from="
                << from);
        }

        const uint32_t receiverNodeId =
            socket->GetNode()->GetId();

        VehicleState* receiver =
            VehicleRegistry::Instance()
                .GetByNodeId(receiverNodeId);

        if (receiver
            && receiver->runtime
            && receiver->runtime->IsInitialized())
        {
            receiver->runtime->ReceivePacket(
                packet,
                from);
        }
    }
}

} // namespace ns3
