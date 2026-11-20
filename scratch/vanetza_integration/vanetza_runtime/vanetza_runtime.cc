#include "vanetza_integration/cpm/types/cpm_message.h"
#include "vanetza_runtime.h"
#include "vanetza_integration/context/station_registry/station_endpoint_registry.h"

#include "vanetza_integration/spatem/rx/spatem_rx_pipeline.h"
#include "vanetza_integration/cpm/rx/cpm_rx_pipeline.h"
#include "vanetza_integration/cpm/transport/cpm_transport_policy.h"
#include "vanetza_integration/vam/transport/vam_transport_policy.h"
#include "vanetza_integration/vam/transport/vam_rx_boundary.h"
#include "vanetza_integration/context/packet/packet_id_manager.h"
#include "vanetza_integration/core/packet_id_tag/packet_id_tag.h"
#include "vanetza_integration/context/rx_bridge/rx_bridge_context.h"
#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/denm/rx/denm_rx_pipeline.h"
#include "vanetza_integration/rx/rx_pipeline.h"
#include "vanetza_integration/rx/rx_context_builder/rx_context_builder.h"
#include "vanetza_integration/wrappers/vanetza/geonet/vanetza_geonet_adapter.h"

#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("VanetzaRuntime");


//=============================================================================
// Hex dump helper
//=============================================================================

static std::string
HexDump(const std::vector<uint8_t>& data, std::size_t maxBytes = 48)
{
    std::ostringstream oss;

    oss << std::hex
        << std::setfill('0');

    const std::size_t limit =
        std::min(maxBytes, data.size());

    for (std::size_t i = 0; i < limit; ++i)
    {
        oss << std::setw(2)
            << static_cast<unsigned>(data[i]);

        if (i % 2 == 1)
        {
            oss << ' ';
        }
    }

    if (limit < data.size())
    {
        oss << "...";
    }

    return oss.str();
}


//=============================================================================
// Constructor
//=============================================================================

VanetzaRuntime::VanetzaRuntime(VehicleState& vehicle)
    : m_vehicle(vehicle),
      m_initialized(false)
{
}


//=============================================================================
// Destructor
//=============================================================================

VanetzaRuntime::~VanetzaRuntime()
{
    Shutdown();
}


//=============================================================================
// DENM credential rotation
//=============================================================================

bool
VanetzaRuntime::RotateDenmCredential()
{
    if (!m_initialized || !m_geonet)
    {
        return false;
    }

    auto& security =
        m_vehicle.security;

    if (security.credentialPool.Empty())
    {
        return false;
    }

    const std::size_t currentIndex =
        security.credentialPool.GetActiveCredentialIndex();

    const std::size_t nextIndex =
        currentIndex + 1;

    const VehicleCredential* nextCredential =
        security.credentialPool.GetCredential(nextIndex);

    if (nextCredential == nullptr)
    {
        return false;
    }

    // Credential ownership is bound to the vehicle logical identity.
    // Rotation must never activate material provisioned for another vehicle.
    if (nextCredential->vehicleId !=
        m_vehicle.identity.vehId)
    {
        return false;
    }

    // Select the new active credential in the vehicle security context.
    // This also refreshes the CAM/OpenSSL material associated with it.
    if (!security.SetActiveDenmCredential(nextIndex))
    {
        return false;
    }

    // The GeoNetworking wrapper owns the actual Vanetza DENM signer, but
    // does not own credential-selection policy. Reload only the signer from
    // the credential already selected by the runtime.
    if (!m_geonet->ReloadDenmCredential())
    {
        // Restore the complete previous security state when signer reload
        // or credential validation fails.
        security.SetActiveDenmCredential(currentIndex);
        return false;
    }

    return true;
}


//=============================================================================
// Initialize
//=============================================================================

bool
VanetzaRuntime::Initialize()
{
    if (m_initialized)
    {
        return true;
    }

    //=========================================================================
    // Identity preconditions
    //
    // VehicleIdentity is authoritative and must already be initialized
    // by VehicleIdentityInitializer before this function is called.
    //=========================================================================

    if (!m_vehicle.node)
    {
        NS_LOG_UNCOND(
            "[VRT_INIT] ERROR: "
            "VanetzaRuntime requires a valid ns-3 Node");

        return false;
    }

    if (m_vehicle.identity.vehId.empty())
    {
        NS_LOG_UNCOND(
            "[VRT_INIT] ERROR: "
            "VanetzaRuntime requires a valid vehicle ID");

        return false;
    }

    if (m_vehicle.identity.stationId == 0)
    {
        NS_LOG_UNCOND(
            "[VRT_INIT] ERROR: "
            "VanetzaRuntime requires a valid stationId");

        return false;
    }

    if (m_vehicle.identity.nodeId !=
        m_vehicle.node->GetId())
    {
        NS_LOG_UNCOND(
            "[VRT_INIT] ERROR: nodeId mismatch: "
            "identity="
            << m_vehicle.identity.nodeId
            << " node="
            << m_vehicle.node->GetId());

        return false;
    }

    //=========================================================================
    // Initialization logging
    //=========================================================================

    NS_LOG_UNCOND(
        "[VRT_INIT] ===========================================");

    NS_LOG_UNCOND(
        "[VRT_INIT] Initializing VanetzaRuntime");

    NS_LOG_UNCOND(
        "[VRT_INIT] Vehicle    : "
        << m_vehicle.identity.vehId);

    NS_LOG_UNCOND(
        "[VRT_INIT] Node       : "
        << m_vehicle.identity.nodeId);

    NS_LOG_UNCOND(
        "[VRT_INIT] Station ID : "
        << m_vehicle.identity.stationId);

    //=========================================================================
    // Vanetza GeoNetworking boundary
    //=========================================================================
    //
    // All direct Vanetza runtime/protocol objects are owned by
    // VanetzaGeoNetAdapter.
    //
    // VanetzaRuntime remains responsible only for orchestration and for
    // connecting the neutral BTP RX result to PacketContext / RxPipeline.
    //=========================================================================

    m_geonet =
        std::make_unique<VanetzaGeoNetAdapter>(
            m_vehicle);

    if (!m_geonet->Initialize())
    {
        NS_LOG_UNCOND(
            "[VRT_INIT] ERROR: VanetzaGeoNetAdapter initialization failed");

        m_geonet.reset();

        return false;
    }

    NS_LOG_UNCOND(
        "[VRT_INIT] VanetzaGeoNetAdapter initialized");

    //=========================================================================
    // BTP RX boundary
    //=========================================================================
    //
    // The callback is deliberately neutral: no Vanetza type crosses into
    // this runtime layer.
    //=========================================================================

    m_geonet->SetBtpRxHandler(
        2001,
        [this](const VanetzaGeoNetAdapter::BtpRxData& rx)
        {
            NS_LOG_UNCOND(
                "[BTP_HANDLER] indicate() translated "
                "to application RxData"
                << " port="
                << rx.destinationPort
                << " payload="
                << rx.payload.size());

            PacketContext* ctx =
                RxBridgeContext::Current();

            if (!ctx)
            {
                NS_LOG_UNCOND(
                    "[BTP_HANDLER] ERROR: "
                    "RxBridgeContext is null");

                return;
            }

            //=================================================================
            // BTP information
            //=================================================================

            ctx->btpDestinationPort = rx.destinationPort;

            ctx->geonet.upperProtocol = rx.upperProtocol;
            ctx->geonet.transportType = rx.transportType;

            ctx->reason =
                "BTP dst_port=" +
                std::to_string(
                    rx.destinationPort);

            //=================================================================
            // GeoNetworking source position
            //=================================================================

            ctx->geonet.hasSourcePosition =
                rx.hasSourcePosition;

            if (rx.hasSourcePosition)
            {
                ctx->geonet.latitude =
                    rx.latitude;

                ctx->geonet.longitude =
                    rx.longitude;
            }

            //=================================================================
            // GeoNetworking transport
            //=================================================================



            //=================================================================
            // Traffic class
            //=================================================================

            ctx->geonet.hasTrafficClass =
                rx.hasTrafficClass;

            if (rx.hasTrafficClass)
            {
                ctx->geonet.trafficClass =
                    rx.trafficClass;
            }

            //=================================================================
            // ITS-AID
            //=================================================================

            ctx->geonet.hasItsAid =
                rx.hasItsAid;

            if (rx.hasItsAid)
            {
                ctx->geonet.itsAid =
                    rx.itsAid;
            }

            //=================================================================
            // Permissions
            //=================================================================

            ctx->geonet.hasPermissions =
                rx.hasPermissions;

            if (rx.hasPermissions)
            {
                ctx->geonet.permissionsSize =
                    rx.permissionsSize;
            }

            //=================================================================
            // Lifetime
            //=================================================================

            ctx->geonet.hasLifetime =
                rx.hasLifetime;

            if (rx.hasLifetime)
            {
                ctx->geonet.lifetimeMs =
                    rx.lifetimeMs;
            }

            //=================================================================
            // GeoNetworking sender MID
            //=================================================================

            ctx->geonet.hasSenderMid =
                rx.hasSourceMid;

            if (rx.hasSourceMid)
            {
                ctx->geonet.senderMid =
                    rx.sourceMid;
            }

            //=================================================================
            // Preserve the existing single PacketContext.
            //=================================================================

            RxPipeline::ProcessDirect(
                *ctx,
                rx.payload);
        });

    //=========================================================================
    // DENM BTP RX boundary
    //=========================================================================
    //
    // Stage 8.7.1:
    // prove that BTP port 2002 reaches a dedicated DENM handler without
    // replacing the CAM handler registered on port 2001.
    //
    // DENM decode/replay/reception are intentionally NOT invoked yet.
    //=========================================================================

    m_geonet->SetBtpRxHandler(
    2002,
    [this](const VanetzaGeoNetAdapter::BtpRxData& rx)
    {
        PacketContext* ctx =
            RxBridgeContext::Current();

        if (!ctx)
        {
            NS_LOG_UNCOND(
                "[DENM_RX] ERROR: RxBridgeContext is null");
            return;
        }

        ctx->btpDestinationPort = rx.destinationPort;

        ctx->geonet.upperProtocol = rx.upperProtocol;
        ctx->geonet.transportType = rx.transportType;

        ctx->geonet.hasItsAid =
            rx.hasItsAid;

        if (rx.hasItsAid)
        {
            ctx->geonet.itsAid =
                rx.itsAid;
        }

        ctx->geonet.hasSenderMid =
            rx.hasSourceMid;

        if (rx.hasSourceMid)
        {
            ctx->geonet.senderMid =
                rx.sourceMid;
        }

        V2XWriteLog(
            *ctx,
            V2XLogLevel::INFO,
            "DENM",
            "DENM_RX_DISPATCH",
            "port=" +
                std::to_string(
                    rx.destinationPort) +
            " payload=" +
                std::to_string(
                    rx.payload.size()) +
            " aid=" +
                std::to_string(
                    rx.hasItsAid
                        ? rx.itsAid
                        : 0));

        DenmRxPipeline::Process(
            *ctx,
            rx.payload);
    });

    //=========================================================================

    //=========================================================================
    // CPM BTP RX boundary
    //
    // Etapa 10.9: dispatch dedicado para o perfil CPM R2.
    // A verificação criptográfica permanece uma responsabilidade separada.
    //=========================================================================

    m_geonet->SetBtpRxHandler(
        kCpmBtpPort,
        [this](const VanetzaGeoNetAdapter::BtpRxData& rx)
        {
            PacketContext* ctx =
                RxBridgeContext::Current();

            if (!ctx)
            {
                NS_LOG_UNCOND(
                    "[CPM_RX] ERROR: RxBridgeContext is null");
                return;
            }

            ctx->btpDestinationPort = rx.destinationPort;

            ctx->geonet.upperProtocol = rx.upperProtocol;
            ctx->geonet.transportType = rx.transportType;

            ctx->geonet.hasItsAid =
                rx.hasItsAid;

            if (rx.hasItsAid)
            {
                ctx->geonet.itsAid =
                    rx.itsAid;
            }

            ctx->geonet.hasSenderMid =
                rx.hasSourceMid;

            if (rx.hasSourceMid)
            {
                ctx->geonet.senderMid =
                    rx.sourceMid;
            }

            V2XWriteLog(
                *ctx,
                V2XLogLevel::INFO,
                "CPM",
                "CPM_RX_DISPATCH",
                "port=" +
                    std::to_string(rx.destinationPort) +
                " payload=" +
                    std::to_string(rx.payload.size()) +
                " aid=" +
                    std::to_string(
                        rx.hasItsAid ? rx.itsAid : 0) +
                " packet_id=" +
                    std::to_string(ctx->packetId));

            // This is local reception metadata, not the CPM reference time
            // and not the ns-3 simulation timestamp.
            const auto receivedAtUnixMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

            CpmMessage message{};

            const bool processed =
                CpmRxPipeline::Process(
                    *ctx,
                    rx.payload,
                    message,
                    receivedAtUnixMs,
                    m_vehicle.security.cpmReplay,
                    m_vehicle.cpmReception);

            NS_LOG_UNCOND(
                "[CPM_RX] BTP handler "
                << (processed ? "accepted" : "rejected")
                << " receiver="
                << m_vehicle.identity.vehId
                << " station="
                << (processed ? message.station.stationId : 0)
                << " objects="
                << (processed ? message.perceivedObjects.size() : 0));
        });

    // SPATEM BTP RX boundary
    // =========================================================================
    //
    // Stage 9.8:
    // BTP port 2004 is the SPATEM application boundary.
    // The specialized SPATEM RX pipeline owns decode and delivery.
    //=========================================================================

    m_geonet->SetBtpRxHandler(
        2004,
        [this](const VanetzaGeoNetAdapter::BtpRxData& rx)
        {
            PacketContext* ctx =
                RxBridgeContext::Current();

            if (!ctx)
            {
                NS_LOG_UNCOND(
                    "[SPATEM_RX] ERROR: RxBridgeContext is null");
                return;
            }

            ctx->btpDestinationPort = rx.destinationPort;

            ctx->geonet.upperProtocol = rx.upperProtocol;
            ctx->geonet.transportType = rx.transportType;

            ctx->geonet.hasItsAid =
                rx.hasItsAid;

            if (rx.hasItsAid)
            {
                ctx->geonet.itsAid =
                    rx.itsAid;
            }

            ctx->geonet.hasSenderMid =
                rx.hasSourceMid;

            if (rx.hasSourceMid)
            {
                ctx->geonet.senderMid =
                    rx.sourceMid;
            }

            V2XWriteLog(
                *ctx,
                V2XLogLevel::INFO,
                "SPATEM",
                "SPATEM_RX_DISPATCH",
                "port=" +
                    std::to_string(
                        rx.destinationPort) +
                " payload=" +
                    std::to_string(
                        rx.payload.size()) +
                " aid=" +
                    std::to_string(
                        rx.hasItsAid
                            ? rx.itsAid
                            : 0));

            SpatemMessage message{};

            const bool processed =
                SpatemRxPipeline::Process(
                    *ctx,
                    rx.payload,
                    message,
                    m_vehicle.security.spatemReplay);

            NS_LOG_UNCOND(
                "[SPATEM_RX] BTP handler "
                << (processed ? "delivered" : "rejected")
                << " receiver="
                << m_vehicle.identity.vehId
                << " port="
                << rx.destinationPort
                << " payload="
                << rx.payload.size()
                << " aid="
                << (rx.hasItsAid ? rx.itsAid : 0));
        });

    //=========================================================================
    // VAM BTP RX boundary — Etapa 11.7
    //
    // Register the dedicated VRU Awareness port without replacing the
    // existing CAM, DENM, CPM or SPATEM handlers. Decode, security policy,
    // replay and LDM delivery belong to their later VAM RX stages.
    //=========================================================================
    m_geonet->SetBtpRxHandler(
        kVamBtpPort,
        [this](const VanetzaGeoNetAdapter::BtpRxData& rx)
        {
            PacketContext* ctx =
                RxBridgeContext::Current();

            if (!ctx)
            {
                NS_LOG_UNCOND(
                    "[VAM_RX] ERROR: RxBridgeContext is null");
                return;
            }

            ctx->btpDestinationPort = rx.destinationPort;

            ctx->geonet.upperProtocol = rx.upperProtocol;
            ctx->geonet.transportType = rx.transportType;

            ctx->geonet.hasSourcePosition =
                rx.hasSourcePosition;
            if (rx.hasSourcePosition)
            {
                ctx->geonet.latitude = rx.latitude;
                ctx->geonet.longitude = rx.longitude;
            }

            ctx->geonet.hasTrafficClass =
                rx.hasTrafficClass;
            if (rx.hasTrafficClass)
            {
                ctx->geonet.trafficClass = rx.trafficClass;
            }

            ctx->geonet.hasItsAid =
                rx.hasItsAid;
            if (rx.hasItsAid)
            {
                ctx->geonet.itsAid = rx.itsAid;
            }

            ctx->geonet.hasPermissions =
                rx.hasPermissions;
            if (rx.hasPermissions)
            {
                ctx->geonet.permissionsSize =
                    rx.permissionsSize;
            }

            ctx->geonet.hasLifetime =
                rx.hasLifetime;
            if (rx.hasLifetime)
            {
                ctx->geonet.lifetimeMs = rx.lifetimeMs;
            }

            ctx->geonet.hasSenderMid =
                rx.hasSourceMid;
            if (rx.hasSourceMid)
            {
                ctx->geonet.senderMid = rx.sourceMid;
            }

            V2XWriteLog(
                *ctx,
                V2XLogLevel::INFO,
                "VAM",
                "VAM_RX_DISPATCH",
                "port=" + std::to_string(rx.destinationPort) +
                    " payload=" + std::to_string(rx.payload.size()) +
                    " aid=" +
                    std::to_string(rx.hasItsAid ? rx.itsAid : 0));

            // Complete only the shared GeoNetworking/BTP RX boundary.
            // VAM verification, decoding, replay and delivery remain in
            // their dedicated later implementation stages.
            const bool accepted =
                VamRxBoundary::Process(*ctx, rx.payload);

            V2XWriteLog(
                *ctx,
                accepted ? V2XLogLevel::INFO : V2XLogLevel::WARN,
                "VAM",
                "VAM_RX_BOUNDARY_RESULT",
                accepted
                    ? "BTP boundary accepted"
                    : "BTP boundary rejected: " + ctx->reason);

            NS_LOG_UNCOND(
                "[VAM_RX] BTP boundary "
                << (accepted ? "accepted" : "rejected")
                << " receiver=" << m_vehicle.identity.vehId
                << " port=" << rx.destinationPort
                << " payload=" << rx.payload.size()
                << " reason=" << ctx->reason);
        });

    //=========================================================================
    // Final initialization state
    //=========================================================================

    NS_LOG_UNCOND(
        "[VRT_INIT] GeoNet adapter : "
        << (m_geonet ? "OK" : "NULL"));

    NS_LOG_UNCOND(
        "[VRT_INIT] Identity authority: "
        "VehicleIdentity");

    NS_LOG_UNCOND(
        "[VRT_INIT] GN_ADDR authority: "
        "VanetzaRuntime");

    NS_LOG_UNCOND(
        "[VRT_INIT] VehicleRegistry role: "
        "reverse indexes only");

    NS_LOG_UNCOND(
        "[VRT_INIT] VanetzaRuntime initialization completed");

    NS_LOG_UNCOND(
        "[VRT_INIT] ===========================================");

    m_initialized = true;

    return true;
}


//=============================================================================
// Shutdown
//=============================================================================

void
VanetzaRuntime::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    m_geonet.reset();

    m_initialized = false;
}


//=============================================================================
// Accessors
//=============================================================================

bool
VanetzaRuntime::IsInitialized() const
{
    return m_initialized;
}


VehicleState&
VanetzaRuntime::Vehicle()
{
    return m_vehicle;
}


bool
VanetzaRuntime::PrepareTx()
{
    if (!m_initialized || !m_geonet)
    {
        NS_LOG_UNCOND(
            "[VRT_TX] ERROR: GeoNetworking adapter not initialized");

        return false;
    }

    return m_geonet->SynchronizeTimeAndPosition();
}


//=============================================================================
// BTP TX
//=============================================================================

bool
VanetzaRuntime::SendBtpB(
    PacketContext& context,
    const BtpTxRequest& request,
    const std::vector<uint8_t>& payload)
{
    if (!m_initialized || !m_geonet)
    {
        NS_LOG_UNCOND(
            "[VRT_TX] ERROR: GeoNetworking adapter not initialized");

        return false;
    }

    return m_geonet->SendBtpB(
        context,
        request,
        payload);
}

bool
VanetzaRuntime::SendBtpB(
    PacketContext& context,
    uint16_t destinationPort,
    const std::vector<uint8_t>& payload)
{
    if (!m_initialized || !m_geonet)
    {
        NS_LOG_UNCOND(
            "[VRT_TX] ERROR: GeoNetworking adapter not initialized");

        return false;
    }

    return m_geonet->SendBtpB(
        context,
        destinationPort,
        payload);
}


//=============================================================================
// Receive path
//=============================================================================

void
VanetzaRuntime::ReceivePacket(
    Ptr<Packet> packet,
    const Address& from)
{
    //-------------------------------------------------------------------------
    // Runtime validation
    //-------------------------------------------------------------------------

    if (!m_initialized || !m_geonet)
    {
        NS_LOG_UNCOND(
            "[VRT_RX] Dropped: runtime not initialized");

        return;
    }

    if (!packet || packet->GetSize() == 0)
    {
        NS_LOG_UNCOND(
            "[VRT_RX] Dropped: null or empty packet");

        return;
    }

    //-------------------------------------------------------------------------
    // Resolve UDP source
    //-------------------------------------------------------------------------

    NS_ABORT_MSG_IF(
        !InetSocketAddress::IsMatchingType(from),
        "[VRT_RX] Unsupported source address type");

    const InetSocketAddress source =
        InetSocketAddress::ConvertFrom(from);

    const Ipv4Address sourceIp =
        source.GetIpv4();

    const StationEndpointIdentity* sender =
        StationEndpointRegistry::Instance().GetByIpv4(sourceIp);

    if (sender == nullptr)
    {
        NS_LOG_UNCOND(
            "[VRT_RX] SENDER_NOT_FOUND "
            "source_ip="
            << sourceIp);

        return;
    }

    NS_ABORT_MSG_IF(
        !sender->linkMacInitialized,
        "[VRT_RX] Sender Link MAC not initialized: "
            << sender->logicalId);

    //-------------------------------------------------------------------------
    // Receive information
    //-------------------------------------------------------------------------

    const uint32_t size =
        packet->GetSize();

    NS_LOG_UNCOND(
        "[VRT_RX] ReceivePacket called, size="
        << size
        << " receiver_veh="
        << m_vehicle.identity.vehId
        << " receiver_node="
        << m_vehicle.identity.nodeId
        << " source_ip="
        << sourceIp
        << " sender_role="
        << StationEndpointKindName(sender->kind)
        << " sender_id="
        << sender->logicalId
        << " sender_veh="
        << sender->vehicleId
        << " sender_node="
        << sender->nodeId
        << " sender_station="
        << sender->stationId
        << " sender_link_mac="
        << sender->linkMac);

    //-------------------------------------------------------------------------
    // Extract complete received frame
    //-------------------------------------------------------------------------

    std::vector<uint8_t> buffer(size);

    packet->CopyData(
        buffer.data(),
        buffer.size());

    NS_LOG_UNCOND(
        "[VRT_RX] First bytes: "
        << HexDump(buffer, 48));

    const std::string rxClassification = m_geonet ? m_geonet->ClassifyRxGeoNetworking(buffer) : "GEONET_ADAPTER_UNAVAILABLE";

    NS_LOG_UNCOND("[VRT_RX] GN classification=" << rxClassification << " size=" << buffer.size());

    if (rxClassification == "BEACON") {
        NS_LOG_UNCOND("[VRT_RX] GN auxiliary BEACON detected; " "no BTP/CAM payload expected");
        return;
    }

    if (!buffer.empty() &&
        (buffer[0] & 0xF0) != 0x10)
    {
        NS_LOG_UNCOND(
            "[VRT_RX] WARNING: First nibble is 0x"
            << std::hex
            << static_cast<int>(buffer[0] >> 4)
            << ", expected 0x1 for GeoNet version 1");
    }

    //-------------------------------------------------------------------------
    // Build the SINGLE RX PacketContext.
    //
    // This context is created at the receive boundary and is preserved
    // through Router -> GeoNetworking -> BTP adapter -> RxPipeline.
    //-------------------------------------------------------------------------

    PacketContext ctx =
        RxContextBuilder::Build(
            0,
            m_vehicle);

    //-------------------------------------------------------------------------
    // Establish sender identity already known at RX boundary.
    //-------------------------------------------------------------------------

    ctx.senderVehId =
        sender->vehicleId;

    ctx.senderNodeId =
        sender->nodeId;

    ctx.senderStationId =
        sender->stationId;

    //-------------------------------------------------------------------------
    // Sender Link MAC.
    //
    // This is the ns-3 Link MAC and is independent from GN_ADDR.MID.
    //-------------------------------------------------------------------------

    ctx.geonet.senderLinkMac =
        sender->linkMac;

    ctx.geonet.hasSenderLinkMac =
        sender->linkMacInitialized;

    ctx.receiverVehId =
        m_vehicle.identity.vehId;

    ctx.receiverNodeId =
        m_vehicle.identity.nodeId;

    ctx.receiverStationId =
        m_vehicle.identity.stationId;

    //-------------------------------------------------------------------------
    // Recover the simulation-only packet ID from PacketIdTag.
    //
    // Packets without a valid tag receive a local fallback ID.
    // vehiclePacketId and sequenceId remain unset on RX because they are
    // internal TX metadata and are not reconstructed from the ASN.1 payload.
    //-------------------------------------------------------------------------


    PacketIdTag rxTag;

    const bool hasPacketIdTag =
        packet->PeekPacketTag(rxTag);

    const auto taggedPacketId =
        hasPacketIdTag ? rxTag.GetPacketId() : 0;

    auto& packetIds =
        PacketIdManager::Instance();

    if (taggedPacketId != 0)
    {
        ctx.packetId = taggedPacketId;

        NS_LOG_UNCOND(
            "[RX_PACKET_ID_TAG] packet_id="
            << ctx.packetId);
    }
    else
    {
        // Fallback: this ID identifies the local RX context only.
        ctx.packetId = packetIds.NextPacketId();

        NS_LOG_UNCOND(
            "[RX_PACKET_ID_TAG_MISSING] reason="
            << (hasPacketIdTag ? "zero_id" : "not_present")
            << " fallback_packet_id="
            << ctx.packetId);
    }

    ctx.vehiclePacketId = 0;
    ctx.sequenceId = 0;

    //-------------------------------------------------------------------------
    // 1/9 RAW_RX
    //-------------------------------------------------------------------------

    ctx.AdvancePipeline(
        PipelineStage::RAW_RX);

    ctx.payload.type =
        PayloadType::RAW;

    ctx.payload.bytes =
        buffer;

    ctx.payload.signatureSize =
        0;

    ctx.reason =
        "RX_FRAME_RECEIVED";

    V2XWriteLog(
        ctx,
        V2XLogLevel::TRACE,
        "RX",
        "RAW_RX",
        "size=" +
            std::to_string(buffer.size()));

    //-------------------------------------------------------------------------
    // 2/9 ACCESS_RX
    //-------------------------------------------------------------------------

    ctx.AdvancePipeline(
        PipelineStage::ACCESS_RX);

    ctx.reason =
        "ACCESS_FRAME_RECEIVED";

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "ACCESS",
        "ACCESS_RX_BEGIN",
        "received_frame_bytes=" +
            std::to_string(buffer.size()));

    ctx.MarkSuccess();

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "ACCESS",
        "ACCESS_RX_OK",
        "bytes=" +
            std::to_string(buffer.size()));

    //-------------------------------------------------------------------------
    // GeoNetworking / Vanetza boundary.
    //
    // PacketContext has already passed through RAW_RX and ACCESS_RX.
    // The adapter owns the Vanetza Router and preserves the existing
    // synchronous Router -> BTP -> RxPipeline path.
    //-------------------------------------------------------------------------

    if (!m_geonet)
    {
        NS_LOG_UNCOND(
            "[VRT_RX] ERROR: GeoNetworking adapter not initialized");

        return;
    }

    if (!m_geonet->Receive(
            ctx,
            buffer,
            sender->linkMac))
    {
        NS_LOG_UNCOND(
            "[VRT_RX] GeoNetworking adapter rejected packet");
    }
}


bool
VanetzaRuntime::TryReceiveCpm(CpmMessage& message)
{
    return m_vehicle.cpmReception.TryPop(message);
}

} // namespace ns3
