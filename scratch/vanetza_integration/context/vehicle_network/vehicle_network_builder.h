#pragma once

#include "ns3/callback.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/node.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/wifi-module.h"

namespace ns3
{

class SpatemEmitterEndpoint;
class CpmEmitterEndpoint;
class VamVruEmitterEndpoint;

struct VehicleState;

class VehicleNetworkBuilder
{
public:

    static void Initialize(
        Ptr<YansWifiChannel> channel,
        const Ipv4AddressHelper& ipv4,
        const Callback<void, Ptr<Socket>>& rxCallback);

    // Inicializa a rede de um endpoint RSU usando o mesmo
    // canal Wi-Fi e o mesmo alocador IPv4 que os veículos.
    static bool InitializeSpatemEmitter(
        SpatemEmitterEndpoint& endpoint);

    static bool InitializeCpmEmitter(
        CpmEmitterEndpoint& endpoint);

    // Reuse the active 802.11p channel and advancing IPv4 allocator for a
    // non-vehicle VAM pedestrian ITS station.
    static bool InitializeVamVruEmitter(
        VamVruEmitterEndpoint& endpoint);

    static Ptr<Socket> Build(
        Ptr<Node> node);

    static void BindLinkMac(
        VehicleState& vehicle);

private:

    static void ConfigureNetworking(
        Ptr<Node> node);

    static Ptr<Socket> CreateSocket(
        Ptr<Node> node);

    static Ptr<YansWifiChannel> m_channel;

    static Ipv4AddressHelper m_ipv4;

    static Callback<void, Ptr<Socket>> m_rxCallback;
};

} // namespace ns3
