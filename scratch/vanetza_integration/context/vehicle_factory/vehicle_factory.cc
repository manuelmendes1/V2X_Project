#include "vehicle_factory.h"

#include "vanetza_integration/context/vehicle_builder/vehicle_builder.h"
#include "vanetza_integration/context/vehicle_network/vehicle_network_builder.h"

#include "ns3/mobility-module.h"

namespace ns3
{

void
VehicleFactory::Initialize(
    Ptr<YansWifiChannel> channel,
    const Ipv4AddressHelper& ipv4,
    const Callback<void, Ptr<Socket>>& rxCallback)
{
    VehicleNetworkBuilder::Initialize(
        channel,
        ipv4,
        rxCallback);
}

void
VehicleFactory::ConfigureMobility(
    Ptr<Node> node)
{
    MobilityHelper mobility;

    mobility.SetMobilityModel(
        "ns3::ConstantVelocityMobilityModel");

    mobility.Install(node);
}

Ptr<Node>
VehicleFactory::CreateVehicle(
    const std::string& vehId)
{
    Ptr<Node> node =
        CreateObject<Node>();

    ConfigureMobility(node);

    /*
     * Primeiro construímos a infraestrutura ns-3:
     *   - Wi-Fi 802.11p
     *   - NetDevice
     *   - IPv4
     *   - UDP socket
     */
    Ptr<Socket> socket =
        VehicleNetworkBuilder::Build(node);

    /*
     * O VehicleBuilder cria o VehicleState permanente e estabelece
     * a identidade definitiva antes de inicializar o VanetzaRuntime.
     */
    VehicleState* state =
        VehicleBuilder::Create(
            vehId,
            node,
            socket);

    NS_ABORT_MSG_IF(
        state == nullptr,
        "[FACTORY] Failed to create VehicleState for vehicle "
            << vehId);

    /*
     * O MAC de Link Layer pertence ao NetDevice ns-3.
     * Só depois de o VehicleState existir podemos materializar
     * essa associação no estado permanente.
     */
    VehicleNetworkBuilder::BindLinkMac(
        *state);

    return node;
}

} // namespace ns3
