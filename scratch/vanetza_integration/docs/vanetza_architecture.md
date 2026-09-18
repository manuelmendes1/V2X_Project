# Vanetza Architecture Reference

> **STATUS: CURRENT**
>
> **TYPE: ARCHITECTURE / API REFERENCE**
>
> The authoritative operational baseline is `CURRENT_STATE.md`.
>
> This document is the current API and architecture reference. Historical implementation names must not be treated as active components.

## Current Architectural Contracts

### Identity

```text
Link MAC ≠ GN_ADDR.MID ≠ stationId

VehicleIdentity
    → identity authority

VanetzaRuntime / GeoNetworking
    → GN_ADDR authority

VehicleRegistry
    → state and reverse indexes
```

### Security

```text
security/
    → operations / algorithms

context/vehicle_security/
    → state / ownership
```

### RX

```text
RAW_RX → ACCESS_RX → GEONET_RX → BTP_RX
→ VERIFY → DECODE → REPLAY → RECEPTION → DELIVER
```

### TX

```text
GENERATE → ENCODE → SIGN → BTP_TX → GEONET_TX
→ ACCESS_TX → SEND → TX_COMPLETE
```

---

# Documento de Arquitetura Vanetza

**Data:** 2026-07-22
**Versao:** 1.0
**Baseado em:** Headers Vanetza /usr/local/include/vanetza/

---

## 1. Visao Geral

O Vanetza implementa a stack ETSI ITS conforme EN 302 636-4-1 (GeoNetworking) e EN 302 636-5-1 (BTP).Aplicacao (CAM)
|
v
BTP-B (porta 2001) --- DataRequestB / DataIndication / PortDispatcher
|
v
GeoNetworking -------- Router::request() / Router::indicate()
|                    ShbDataRequest / GbcDataRequest
v
Access Layer --------- DataRequest (ether_type, MACs)
|
v
802.11p (ns-3::Packet)
plain

---

## 2. GeoNetworking (vanetza/geonet/)

### 2.1 Router (/usr/local/include/vanetza/geonet/router.hpp)

Classe central do GeoNetworking.

```cpp
namespace vanetza::geonet {

class Router {
public:
    typedef std::unique_ptr<DataRequest> DataRequestPtr;
    typedef std::unique_ptr<Pdu> PduPtr;
    typedef std::unique_ptr<DownPacket> DownPacketPtr;
    typedef std::unique_ptr<UpPacket> UpPacketPtr;

    // Construtor
    Router(Runtime&, const MIB&);

    // TX: Pedidos de transmissao
    DataConfirm request(const ShbDataRequest&, DownPacketPtr);
    DataConfirm request(const GbcDataRequest&, DownPacketPtr);
    DataConfirm request(const GucDataRequest&, DownPacketPtr);   // NAO SUPORTADO
    DataConfirm request(const GacDataRequest&, DownPacketPtr);   // NAO SUPORTADO
    DataConfirm request(const TsbDataRequest&, DownPacketPtr);   // NAO SUPORTADO

    // RX: Rececao de pacote da access layer
    void indicate(UpPacketPtr, const MacAddress& sender, const MacAddress& destination);

    // Hooks
    Hook<PacketDropReason> packet_dropped;
    Hook<ForwardingStopReason> forwarding_stopped;

    // Configuracao
    void update_position(const PositionFix&);
    void set_transport_handler(UpperProtocol proto, TransportInterface* ifc);
    void set_security_entity(security::SecurityEntity* entity);
    void set_access_interface(dcc::RequestInterface* ifc);
    void set_dcc_field_generator(DccFieldGenerator* dcc);
    void set_address(const Address&);
    void set_random_seed(std::uint_fast32_t seed);

    // Getters
    const MIB& get_mib() const;
    const CbfPacketBuffer& get_cbf_buffer() const;
    const LocationTable& get_location_table() const;
    const LongPositionVector& get_local_position_vector() const;
};

} // namespace vanetza::geonet
2.2 DataRequest (/usr/local/include/vanetza/geonet/data_request.hpp)
cpp
namespace vanetza::geonet {

struct DataRequest {
    DataRequest(const MIB& mib, ItsAid its_aid = 0);

    struct Repetition {
        units::Duration interval;
        units::Duration maximum;
    };

    UpperProtocol upper_protocol;           // BTP_A ou BTP_B
    CommunicationProfile communication_profile;
    ItsAid its_aid;
    ByteBuffer permissions;
    Lifetime maximum_lifetime;
    boost::optional<Repetition> repetition;
    unsigned max_hop_limit;
    TrafficClass traffic_class;
};

struct DataRequestWithAddress : public DataRequest {
    Address destination;
};

struct DataRequestWithArea : public DataRequest {
    Area destination;
};

struct GucDataRequest : public DataRequestWithAddress {};
struct GbcDataRequest : public DataRequestWithArea {};
struct GacDataRequest : public DataRequestWithArea {};
struct ShbDataRequest : public DataRequest {};
struct TsbDataRequest : public DataRequest {};

using DataRequestVariant = boost::variant<
    GucDataRequest, GbcDataRequest, GacDataRequest, ShbDataRequest, TsbDataRequest
>;

// Copia parametros BTP-B para GeoNet
void copy_request_parameters(const btp::DataRequestB& btp, DataRequest& gn);
void copy_request_parameters(const btp::DataRequestB& btp, DataRequestWithAddress& gn);
void copy_request_parameters(const btp::DataRequestB& btp, DataRequestWithArea& gn);

} // namespace vanetza::geonet
2.3 Packet (/usr/local/include/vanetza/geonet/packet.hpp)
Alias para vanetza::DownPacket e vanetza::UpPacket (definidos em vanetza/net/packet.hpp).
cpp
namespace vanetza::geonet {
    using vanetza::DownPacket;
    using vanetza::UpPacket;
}
2.4 TransportInterface (/usr/local/include/vanetza/geonet/transport_interface.hpp)
cpp
namespace vanetza::geonet {

struct DataIndication;

class TransportInterface {
public:
    virtual void indicate(const DataIndication&, std::unique_ptr<UpPacket>) = 0;
    virtual ~TransportInterface() {}
};

} // namespace vanetza::geonet
2.5 MIB (/usr/local/include/vanetza/geonet/mib.hpp)
cpp
namespace vanetza::geonet {

struct ManagementInformationBase {
    ManagementInformationBase();

    Address itsGnLocalGnAddr;
    AddrConfMethod itsGnLocalAddrConfMethod;
    unsigned itsGnProtocolVersion;
    bool itsGnIsMobile;
    InterfaceType itsGnIfType;
    units::Frequency itsGnMinimumUpdateFrequencyEPV;
    units::Length itsGnPaiInterval;
    unsigned itsGnMaxSduSize;
    unsigned itsGnMaxGeoNetworkingHeaderSize;
    units::Duration itsGnLifetimeLocTE;
    bool itsGnSecurity;
    SecurityDecapHandling itsGnSnDecapResultHandling;
    unsigned itsGnLocationServiceMaxRetrans;
    units::Duration itsGnLocationServiceRetransmitTimer;
    unsigned itsGnLocationServicePacketBufferSize;
    units::Duration itsGnBeaconServiceRetransmitTimer;
    units::Duration itsGnBeaconServiceMaxJitter;
    unsigned itsGnDefaultHopLimit;
    unsigned itsGnDPLLength;
    Lifetime itsGnMaxPacketLifetime;
    Lifetime itsGnDefaultPacketLifetime;
    unsigned itsGnMaxPacketDataRate;
    double itsGnMaxPacketDataRateEmaBeta;
    units::Area itsGnMaxGeoAreaSize;
    units::Duration itsGnMinPacketRepetitionInterval;
    UnicastForwarding itsGnNonAreaForwardingAlgorithm;
    BroadcastForwarding itsGnAreaForwardingAlgorithm;
    units::Duration itsGnCbfMinTime;
    units::Duration itsGnCbfMaxTime;
    units::Length itsGnDefaultMaxCommunicationRange;
    units::Angle itsGnBroadcastCBFDefSectorAngle;
    unsigned itsGnUcForwardingPacketBufferSize;
    unsigned itsGnBcForwardingPacketBufferSize;
    unsigned itsGnCbfPacketBufferSize;
    TrafficClass itsGnDefaultTrafficClass;
    std::uint32_t vanetzaDefaultSeed;
    std::size_t vanetzaCbfMaxCounter;
    Clock::duration vanetzaDeferInitialBeacon;
    bool vanetzaDisableBeaconing;
    bool vanetzaMultiHopDuplicateAddressDetection;
    bool vanetzaFadingCbfCounter;
    units::Duration vanetzaFadingCbfCounterLifetime;
    Clock::duration vanetzaNeighbourFlagExpiry;
    std::size_t vanetzaGbcMemoryCapacity;
    bool vanetzaGbcPassUpOutsideDestination;
};

typedef ManagementInformationBase MIB;

} // namespace vanetza::geonet
3. BTP (vanetza/btp/)
3.1 DataRequest (/usr/local/include/vanetza/btp/data_request.hpp)
cpp
namespace vanetza::btp {

struct DataRequestGeoNetParams {
    geonet::TransportType transport_type;
    geonet::DestinationVariant destination;
    geonet::CommunicationProfile communication_profile;
    ItsAid its_aid;
    boost::optional<geonet::Lifetime> maximum_lifetime;
    boost::optional<unsigned> maximum_hop_limit;
    boost::optional<geonet::DataRequest::Repetition> repetition;
    geonet::TrafficClass traffic_class;
};

struct DataRequestA {
    DataRequestA();
    decltype(HeaderA::destination_port) destination_port;
    decltype(HeaderA::source_port) source_port;
    DataRequestGeoNetParams gn;
};

struct DataRequestB {
    DataRequestB();
    decltype(HeaderB::destination_port) destination_port;
    decltype(HeaderB::destination_port_info) destination_port_info;
    DataRequestGeoNetParams gn;
};

} // namespace vanetza::btp
3.2 DataIndication (/usr/local/include/vanetza/btp/data_indication.hpp)
cpp
namespace vanetza::btp {

struct DataIndication {
    DataIndication();
    DataIndication(const geonet::DataIndication&, const HeaderA&);
    DataIndication(const geonet::DataIndication&, const HeaderB&);

    boost::optional<port_type> source_port;
    port_type destination_port;
    boost::optional<decltype(HeaderB::destination_port_info)> destination_port_info;
    decltype(geonet::DataIndication::destination) destination;
    decltype(geonet::DataIndication::its_aid) its_aid;
    decltype(geonet::DataIndication::permissions) permissions;
    geonet::ShortPositionVector source_position;
    geonet::TrafficClass traffic_class;
    boost::optional<geonet::Lifetime> remaining_packet_lifetime;
};

} // namespace vanetza::btp
3.3 PortDispatcher (/usr/local/include/vanetza/btp/port_dispatcher.hpp)
cpp
namespace vanetza::btp {

class PortDispatcher : public geonet::TransportInterface {
public:
    class PromiscuousHook {
    public:
        virtual void tap_packet(const DataIndication&, const UpPacket&) = 0;
        virtual ~PromiscuousHook() {}
    };

    void set_interactive_handler(port_type, IndicationInterface*);
    void set_non_interactive_handler(port_type, IndicationInterface*);
    void add_promiscuous_hook(PromiscuousHook* hook);
    void remove_promiscuous_hook(PromiscuousHook*);

    // Implementacao de geonet::TransportInterface
    void indicate(const geonet::DataIndication&, std::unique_ptr<UpPacket>) override;

    Hook<const geonet::DataIndication&, const btp::DataIndication*> hook_undispatched;
};

} // namespace vanetza::btp
3.4 Header (/usr/local/include/vanetza/btp/header.hpp)
cpp
namespace vanetza::btp {

typedef uint16be_t port_type;

struct HeaderA {
    static constexpr std::size_t length_bytes = 4;
    port_type destination_port;
    port_type source_port;
};

struct HeaderB {
    static constexpr std::size_t length_bytes = 4;
    port_type destination_port;
    uint16be_t destination_port_info;
};

} // namespace vanetza::btp
4. Access Layer (vanetza/access/)
4.1 DataRequest (/usr/local/include/vanetza/access/data_request.hpp)
cpp
namespace vanetza::access {

struct DataRequest {
    DataRequest() : access_category(AccessCategory::BK) {}

    uint16be_t ether_type;
    MacAddress source_addr;
    MacAddress destination_addr;
    AccessCategory access_category;
};

} // namespace vanetza::access
5. Diagrama de Sequencia TX
plain
Current CAM Scheduler / CAM generation path
    |
    v
[ENCODE + SIGN] → payload (CAM assinado)
    |
    v
VanetzaBtpAdapter::SendBtpB()
    |
    +-- const DataRequestB& req
    |      destination_port = 2001
    |      destination_port_info = 0
    |      gn.transport_type = SHB
    |      gn.destination = (none, SHB nao tem destino)
    |
    v
Vanetza geonet::Router::request() via VanetzaBtpAdapter
    |
    +-- ShbDataRequest shb_req(mib)
    |      copy_request_parameters(btp_req, shb_req)
    |
    +-- DownPacketPtr packet = create_down_packet(payload)
    |
    v
Router::request(shb_req, move(packet))
    |
    v
[Router constroi GN header: Basic + Common + SHB Extended]
    |
    v
Ns3AccessAdapter TX boundary
    |
    +-- extrai bytes do DownPacket
    +-- cria ns3::Packet
    |
    v
ns3::Socket::SendTo(ns3::Packet, broadcast)
6. Diagrama de Sequencia RX
plain
ns3::Socket::RecvFrom()
    |
    v
Ns3AccessAdapter RX boundary
    |
    +-- cria UpPacket a partir de ns3::Packet
    |
    v
Router::indicate(up_packet, sender_mac, dest_mac)
    |
    v
[Router parse: Basic Header → Common Header → Extended Header (SHB)]
    |
    v
[Router verifica: hop_limit, duplicados, security]
    |
    v
Router::pass_up(DataIndication, UpPacketPtr)
    |
    v
PortDispatcher::indicate() [registado como TransportInterface]
    |
    +-- parse BTP header (A ou B)
    +-- demux por porta
    |
    v
IndicationInterface::indicate() [handler porta 2001]
    |
    v
BTP RX dispatch via the Vanetza Router transport handler
    |
    v
[Extrai payload CAM assinado]
    |
    v
Current RX pipeline: RAW_RX → ACCESS_RX → GEONET_RX → BTP_RX → VERIFY → DECODE → REPLAY → RECEPTION → DELIVER
    |
    v
VERIFY → DECODE → REPLAY → DELIVER
7. Dependencias de Compilacao
Table
Modulo Vanetza	Headers Usados	Libs
geonet	router.hpp, data_request.hpp, packet.hpp, transport_interface.hpp, mib.hpp	libvanetza_geonet.so
btp	data_request.hpp, data_indication.hpp, port_dispatcher.hpp, header.hpp	libvanetza_btp.so
access	data_request.hpp	libvanetza_access.so
common	byte_buffer.hpp, clock.hpp	libvanetza_common.so
net	packet.hpp, mac_address.hpp	libvanetza_net.so
8. Notas de Implementacao
SHB vs GBC: Para CAMs usamos SHB (Single Hop Broadcast) — nao ha destino especifico, broadcast na area.
Porta 2001: Porta BTP-B padrao para CAMs (ETSI TS 102 965).
UpperProtocol: BTP-B = valor 2 (nao BTP-A = 1).
copy_request_parameters(): Funcao utilitaria que copia parametros do BTP para o GeoNet.
DownPacket/UpPacket: Definidos em vanetza/net/packet.hpp. Usam ByteBuffer internamente.
MacAddress: Definido em vanetza/net/mac_address.hpp.
ether_type: vanetza::access::ether_type para GeoNetworking (0x8947).
Security: O Router pode encapsular/decapsular security envelopes se set_security_entity() for chamado. Nesta fase, usamos security da nossa stack (ECDSA P-256) e desativamos security do Vanetza.
Documento gerado a partir da inspecao direta dos headers Vanetza instalados em /usr/local/include/vanetza/
