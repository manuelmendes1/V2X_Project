#include "vehicle_network_builder.h"
#include "vanetza_integration/core/phy_audit/phy_audit.h"
#include "vanetza_integration/spatem/emitter/spatem_emitter_endpoint.h"
#include "vanetza_integration/cpm/emitter/cpm_emitter_endpoint.h"
#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"

#include "ns3/inet-socket-address.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/socket.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-module.h"

namespace ns3
{

Ptr<YansWifiChannel>
VehicleNetworkBuilder::m_channel = nullptr;

Ipv4AddressHelper
VehicleNetworkBuilder::m_ipv4{};

Callback<void, Ptr<Socket>>
VehicleNetworkBuilder::m_rxCallback{};

// ============================================================================
// Initialize
// ============================================================================

void
VehicleNetworkBuilder::Initialize(
    Ptr<YansWifiChannel> channel,
    const Ipv4AddressHelper& ipv4,
    const Callback<void, Ptr<Socket>>& rxCallback)
{
    m_channel = channel;
    m_ipv4 = ipv4;
    m_rxCallback = rxCallback;
}

bool
VehicleNetworkBuilder::InitializeSpatemEmitter(
    SpatemEmitterEndpoint& endpoint)
{
    if (m_channel == nullptr)
    {
        NS_LOG_UNCOND(
            "[NET] Cannot initialize SPATEM emitter: Wi-Fi channel is null");
        return false;
    }

    // Este método usa m_ipv4 diretamente. Não cria uma cópia do
    // alocador já avançado pelos endereços atribuídos aos veículos.
    Callback<void, Ptr<Socket>> noReceiveCallback;

    return endpoint.InitializeNetwork(
        m_channel,
        m_ipv4,
        noReceiveCallback);
}

bool
VehicleNetworkBuilder::InitializeCpmEmitter(
    CpmEmitterEndpoint& endpoint)
{
    if (m_channel == nullptr)
    {
        NS_LOG_UNCOND(
            "[NET] Cannot initialize CPM emitter: Wi-Fi channel is null");
        return false;
    }

    // Reuse the active allocator; do not duplicate already assigned IPv4s.
    Callback<void, Ptr<Socket>> noReceiveCallback;

    return endpoint.InitializeNetwork(
        m_channel,
        m_ipv4,
        noReceiveCallback);
}

// ============================================================================
// Build
// ============================================================================

Ptr<Socket>
VehicleNetworkBuilder::Build(
    Ptr<Node> node)
{
    NS_ABORT_MSG_IF(
        node == nullptr,
        "[NET] Cannot build network: node is null");

    ConfigureNetworking(node);

    return CreateSocket(node);
}

// ============================================================================
// Bind Link MAC to permanent VehicleState
// ============================================================================

void
VehicleNetworkBuilder::BindLinkMac(
    VehicleState& vehicle)
{
    NS_ABORT_MSG_IF(
        vehicle.node == nullptr,
        "[NET] Cannot bind Link MAC: vehicle node is null");

    const uint32_t deviceCount =
        vehicle.node->GetNDevices();

    NS_ABORT_MSG_IF(
        deviceCount == 0,
        "[NET] Cannot bind Link MAC: vehicle has no NetDevice");

    bool foundWifi = false;

    for (uint32_t i = 0; i < deviceCount; ++i)
    {
        Ptr<NetDevice> device =
            vehicle.node->GetDevice(i);

        if (device == nullptr)
        {
            continue;
        }

        Ptr<WifiNetDevice> wifiDevice =
            DynamicCast<WifiNetDevice>(device);

        if (wifiDevice == nullptr)
        {
            continue;
        }

        const Address address =
            wifiDevice->GetAddress();

        NS_ABORT_MSG_IF(
            !Mac48Address::IsMatchingType(address),
            "[NET] WifiNetDevice does not expose Mac48Address");

        vehicle.linkMac =
            Mac48Address::ConvertFrom(address);

        vehicle.linkMacInitialized = true;

        Ptr<Ipv4> ipv4 =
            vehicle.node->GetObject<Ipv4>();

        NS_ABORT_MSG_IF(
            ipv4 == nullptr,
            "[NET] Vehicle node has no Ipv4 object");

        const int32_t interfaceIndex =
            ipv4->GetInterfaceForDevice(
                wifiDevice);

        NS_ABORT_MSG_IF(
            interfaceIndex < 0,
            "[NET] Could not resolve IPv4 interface for WifiNetDevice");

        NS_ABORT_MSG_IF(
            ipv4->GetNAddresses(interfaceIndex) == 0,
            "[NET] IPv4 interface has no address");

        vehicle.ipv4Address =
            ipv4->GetAddress(
                interfaceIndex,
                0).GetLocal();

        vehicle.ipv4Initialized = true;

        NS_LOG_UNCOND(
            "[NET] node="
            << vehicle.identity.nodeId
            << " veh="
            << vehicle.identity.vehId
            << " wifiDeviceIndex="
            << i
            << " ip="
            << vehicle.ipv4Address
            << " linkMac="
            << vehicle.linkMac);

        VehicleRegistry::Instance().RegisterLinkMac(vehicle.linkMac, vehicle);

        VehicleRegistry::Instance().RegisterIpv4(vehicle.ipv4Address, vehicle);

        NS_ABORT_MSG_IF(
            !StationEndpointRegistry::Instance().RegisterVehicle(
                vehicle.identity.vehId,
                vehicle.identity.nodeId,
                vehicle.identity.stationId,
                vanetza::geonet::StationType::Passenger_Car,
                vehicle.ipv4Address,
                vehicle.linkMac,
                vehicle.linkMacInitialized),
            "[STATION_REGISTRY] Failed to register vehicle IPv4 endpoint: "
                << vehicle.identity.vehId);

        foundWifi = true;
        break;
    }

    NS_ABORT_MSG_IF(
        !foundWifi,
        "[NET] No WifiNetDevice found for vehicle "
            << vehicle.identity.vehId);
}


// ============================================================================
// Configure networking
// ============================================================================

void
VehicleNetworkBuilder::ConfigureNetworking(
    Ptr<Node> node)
{
    NS_ABORT_MSG_IF(
        node == nullptr,
        "[NET] Cannot configure networking: node is null");

    InternetStackHelper internet;
    internet.Install(node);

    YansWifiPhyHelper phy;
    WifiHelper wifi;
    WifiMacHelper mac;

    phy.SetChannel(m_channel);

    wifi.SetStandard(WIFI_STANDARD_80211p);

    wifi.SetRemoteStationManager(
        "ns3::ConstantRateWifiManager",
        "DataMode",
        StringValue("OfdmRate6MbpsBW10MHz"),
        "ControlMode",
        StringValue("OfdmRate6MbpsBW10MHz"));

    mac.SetType("ns3::AdhocWifiMac");



    NetDeviceContainer devices =
        wifi.Install(
            phy,
            mac,
            node);

    PhyAudit::ConnectVehicle(node, devices);

    TrafficControlHelper tch;

    tch.SetRootQueueDisc(
        "ns3::PfifoFastQueueDisc");

    tch.Install(devices);


    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<NetDevice> device = devices.Get(i);

        NS_ABORT_MSG_IF(
            device == nullptr,
            "[NET] Null NetDevice at index "
                << i);

        const Address address =
            device->GetAddress();

        NS_LOG_UNCOND(
            "[NET_DEBUG] node="
            << node->GetId()
            << " device="
            << i
            << " type="
            << device->GetInstanceTypeId().GetName()
            << " address="
            << address);
    }



    /*
     * Assign IPv4 only once.
     */
    Ipv4InterfaceContainer interfaces =
        m_ipv4.Assign(devices);

    NS_LOG_UNCOND(
        "[NET] node="
        << node->GetId()
        << " ip="
        << interfaces.GetAddress(0));
}

// ============================================================================
// Create socket
// ============================================================================

Ptr<Socket>
VehicleNetworkBuilder::CreateSocket(
    Ptr<Node> node)
{
    NS_ABORT_MSG_IF(
        node == nullptr,
        "[NET] Cannot create socket: node is null");

    Ptr<Socket> socket =
        Socket::CreateSocket(
            node,
            UdpSocketFactory::GetTypeId());

    NS_ABORT_MSG_IF(
        socket == nullptr,
        "[NET] Failed to create UDP socket");

    socket->SetAllowBroadcast(true);

    const int result =
        socket->Bind(
            InetSocketAddress(
                Ipv4Address::GetAny(),
                5000));

    NS_ABORT_MSG_IF(
        result != 0,
        "[NET] Failed to bind UDP socket on port 5000");

    socket->SetRecvCallback(
        m_rxCallback);

    return socket;
}

} // namespace ns3
