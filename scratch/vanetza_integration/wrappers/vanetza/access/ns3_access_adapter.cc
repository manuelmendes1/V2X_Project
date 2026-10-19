#include "ns3_access_adapter.h"

#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/core/packet_id_tag/packet_id_tag.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"

#include <vanetza/access/data_request.hpp>
#include <vanetza/common/archives.hpp>
#include <vanetza/common/byte_buffer.hpp>
#include <vanetza/common/its_aid.hpp>
#include <vanetza/geonet/address.hpp>
#include <vanetza/geonet/basic_header.hpp>
#include <vanetza/geonet/beacon_header.hpp>
#include <vanetza/geonet/common_header.hpp>
#include <vanetza/geonet/dcc_field.hpp>
#include <vanetza/geonet/header_type.hpp>
#include <vanetza/geonet/header_variant.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/geonet/parser.hpp>
#include <vanetza/geonet/position_vector.hpp>
#include <vanetza/geonet/router.hpp>
#include <vanetza/geonet/timestamp.hpp>
#include <vanetza/net/chunk_packet.hpp>
#include <vanetza/net/osi_layer.hpp>
#include <vanetza/security/v3/secured_message.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>


namespace ns3 {

namespace {

    const char* LayerName(vanetza::OsiLayer layer)
    {
        switch (layer)
        {
            case vanetza::OsiLayer::Physical:
                return "Physical";

            case vanetza::OsiLayer::Link:
                return "Link";

            case vanetza::OsiLayer::Network:
                return "Network";

            case vanetza::OsiLayer::Transport:
                return "Transport";

            case vanetza::OsiLayer::Session:
                return "Session";

            case vanetza::OsiLayer::Presentation:
                return "Presentation";

            case vanetza::OsiLayer::Application:
                return "Application";
        }

        return "Unknown";
    }


    std::string
    HexDump(
        const std::vector<uint8_t>& data,
        std::size_t bytesPerLine = 16)
    {
        std::ostringstream oss;

        oss << std::hex << std::setfill('0');

        for (std::size_t i = 0;
             i < data.size();
             ++i)
        {
            if (i % bytesPerLine == 0)
            {
                if (i != 0)
                {
                    oss << '\n';
                }

                oss << std::setw(4)
                    << i
                    << ": ";
            }

            oss << std::setw(2)
                << static_cast<unsigned>(data[i])
                << ' ';
        }

        return oss.str();
    }


    std::string
    NextHeaderBasicToString(uint8_t nh)
    {
        switch (nh)
        {
            case 0:
                return "ANY";

            case 1:
                return "COMMON_HEADER";

            case 2:
                return "SECURED_PACKET";

            default:
                return "UNKNOWN";
        }
    }


    const char*
    CommonNextHeaderToString(uint8_t nh)
    {
        switch (nh)
        {
            case 0:
                return "ANY";

            case 1:
                return "BTP_A";

            case 2:
                return "BTP_B";

            case 3:
                return "IPv6";

            default:
                return "UNKNOWN";
        }
    }


    const char*
    GeoNetworkingHeaderTypeToString(uint8_t type)
    {
        switch (type)
        {
            case 0x00:
                return "ANY";

            case 0x10:
                return "BEACON";

            case 0x20:
                return "GEO_UNICAST";

            case 0x30:
                return "GEO_ANYCAST_CIRCLE";

            case 0x31:
                return "GEO_ANYCAST_RECT";

            case 0x32:
                return "GEO_ANYCAST_ELIP";

            case 0x40:
                return "GEO_BROADCAST_CIRCLE";

            case 0x41:
                return "GEO_BROADCAST_RECT";

            case 0x42:
                return "GEO_BROADCAST_ELIP";

            case 0x50:
                return "TSB_SINGLE_HOP";

            case 0x51:
                return "TSB_MULTI_HOP";

            case 0x60:
                return "LS_REQUEST";

            case 0x61:
                return "LS_REPLY";

            default:
                return "UNKNOWN";
        }
    }


    /**
     * Extract the GeoNetworking payload contained in a secured V3 packet.
     *
     * Wire layout:
     *
     *     Basic Header
     *         ->
     *     SecuredMessage V3
     *         ->
     *     SecuredMessage::payload()
     *         ->
     *     GeoNetworking payload
     *
     * No fixed SecuredMessage size or fixed Common Header offset is used.
     */
    bool
    ExtractSecuredV3GeoNetworkingPayload(
        const std::vector<uint8_t>& networkBytes,
        vanetza::ByteBuffer& geoNetworkingPayload,
        vanetza::ItsAid& itsAid)
    {
        constexpr std::size_t basicHeaderSize = 4;

        geoNetworkingPayload.clear();
        itsAid = 0;

        if (networkBytes.size() <= basicHeaderSize)
        {
            return false;
        }

        const uint8_t firstOctet =
            networkBytes[0];

        const uint8_t version =
            (firstOctet >> 4) & 0x0F;

        const uint8_t nextHeader =
            firstOctet & 0x0F;

        if (version != 1 ||
            nextHeader != 2)
        {
            return false;
        }

        const std::vector<uint8_t> securedBytes(
            networkBytes.begin() + basicHeaderSize,
            networkBytes.end());

        if (securedBytes.empty())
        {
            return false;
        }

        const std::string serialized(
            reinterpret_cast<const char*>(
                securedBytes.data()),
            securedBytes.size());

        std::istringstream input(
            serialized,
            std::ios::binary);

        vanetza::InputArchive archive(input);

        vanetza::security::v3::SecuredMessage securedMessage;

        try
        {
            const std::size_t consumed =
                vanetza::security::v3::deserialize(
                    archive,
                    securedMessage);

            if (consumed == 0)
            {
                return false;
            }

            if (!archive.is_good())
            {
                return false;
            }

            if (archive.remaining_bytes() != 0)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        itsAid =
            securedMessage.its_aid();

        const vanetza::PacketVariant payload =
            securedMessage.payload();

        boost::apply_visitor(
            [&geoNetworkingPayload](const auto& packet)
            {
                using Packet =
                    std::decay_t<decltype(packet)>;

                if constexpr (
                    std::is_same_v<
                        Packet,
                        vanetza::CohesivePacket>)
                {
                    const auto range =
                        packet[
                            vanetza::OsiLayer::Network];

                    geoNetworkingPayload.assign(
                        range.begin(),
                        range.end());
                }
                else
                {
                    packet[
                        vanetza::OsiLayer::Network]
                        .convert(geoNetworkingPayload);
                }
            },
            payload);

        return !geoNetworkingPayload.empty();
    }


    /**
     * Validate a GeoNetworking Beacon payload.
     *
     * The supplied bytes start at the Common Header, i.e. the Basic Header
     * and secured envelope have already been removed.
     */
    bool
    ValidateBeaconPayloadTx(
        const vanetza::ByteBuffer& geoNetworkingPayload)
    {
        constexpr std::size_t commonHeaderSize = 8;
        constexpr std::size_t beaconHeaderSize = 24;
        constexpr std::size_t expectedMinimumSize =
            commonHeaderSize + beaconHeaderSize;

        if (geoNetworkingPayload.size() <
            expectedMinimumSize)
        {
            NS_LOG_UNCOND(
                "[GEONET_BEACON_TX] RESULT=FAIL "
                "reason=PAYLOAD_TOO_SHORT "
                "size="
                << geoNetworkingPayload.size()
                << " expected_min="
                << expectedMinimumSize);

            return false;
        }

        vanetza::geonet::Parser parser(
            geoNetworkingPayload.begin(),
            geoNetworkingPayload.end());

        vanetza::geonet::CommonHeader common;

        if (parser.parse_common(common) == 0)
        {
            NS_LOG_UNCOND(
                "[GEONET_BEACON_TX] RESULT=FAIL "
                "reason=COMMON_HEADER_PARSE_FAILED");

            return false;
        }

        const uint8_t headerType =
            static_cast<uint8_t>(
                common.header_type);

        if (common.header_type !=
            vanetza::geonet::HeaderType::Beacon)
        {
            NS_LOG_UNCOND(
                "[GEONET_BEACON_TX] RESULT=FAIL "
                "reason=NOT_BEACON "
                "header_type=0x"
                << std::hex
                << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned>(headerType)
                << std::dec
                << " name="
                << GeoNetworkingHeaderTypeToString(
                       headerType));

            return false;
        }

        if (common.payload != 0)
        {
            NS_LOG_UNCOND(
                "[GEONET_BEACON_TX] RESULT=FAIL "
                "reason=BEACON_PAYLOAD_NOT_ZERO "
                "payload="
                << common.payload);

            return false;
        }

        vanetza::geonet::HeaderVariant extended;

        const auto parsed =
            parser.parse_extended(
                extended,
                vanetza::geonet::HeaderType::Beacon);

        if (!parsed ||
            parser.parsed_bytes() <
                expectedMinimumSize)
        {
            NS_LOG_UNCOND(
                "[GEONET_BEACON_TX] RESULT=FAIL "
                "reason=BEACON_HEADER_PARSE_FAILED "
                "parsed_bytes="
                << parser.parsed_bytes()
                << " expected_min="
                << expectedMinimumSize);

            return false;
        }

        NS_LOG_UNCOND(
            "[GEONET_BEACON_TX] "
            "common_header=PASS "
            "header_type="
            << GeoNetworkingHeaderTypeToString(
                   headerType)
            << " common_payload="
            << common.payload
            << " beacon_size="
            << beaconHeaderSize
            << " parsed_bytes="
            << parser.parsed_bytes()
            << " payload_size="
            << geoNetworkingPayload.size()
            << " RESULT=PASS");

        return true;
    }

} // anonymous namespace


Ptr<WifiPhy>
Ns3AccessAdapter::GetWifiPhy() const
{
    NS_ABORT_MSG_IF(
        m_node == nullptr,
        "[DCC] Vehicle has no node");

    for (uint32_t i = 0;
         i < m_node->GetNDevices();
         ++i)
    {
        Ptr<WifiNetDevice> wifiDevice =
            DynamicCast<WifiNetDevice>(
                m_node->GetDevice(i));

        if (wifiDevice != nullptr)
        {
            Ptr<WifiPhy> phy =
                wifiDevice->GetPhy();

            NS_ABORT_MSG_IF(
                phy == nullptr,
                "[DCC] WifiNetDevice has no WifiPhy");

            return phy;
        }
    }

    NS_ABORT_MSG(
        "[DCC] No WifiNetDevice found for vehicle");

    return nullptr;
}

bool
Ns3AccessAdapter::ValidateSecuredMessageV3Tx(
    const std::vector<uint8_t>& networkBytes) const
{
    constexpr std::size_t basicHeaderSize = 4;

    if (networkBytes.size() <= basicHeaderSize)
    {
        NS_LOG_UNCOND(
            "[SECURED_TX] RESULT=FAIL "
            "reason=NO_SECURED_PAYLOAD "
            "network_size="
            << networkBytes.size());

        return false;
    }

    const uint8_t firstOctet =
        networkBytes[0];

    const uint8_t version =
        (firstOctet >> 4) & 0x0F;

    const uint8_t nextHeader =
        firstOctet & 0x0F;

    if (version != 1)
    {
        NS_LOG_UNCOND(
            "[SECURED_TX] RESULT=FAIL "
            "reason=INVALID_GN_VERSION "
            "version="
            << static_cast<unsigned>(version));

        return false;
    }

    if (nextHeader != 2)
    {
        NS_LOG_UNCOND(
            "[SECURED_TX] RESULT=FAIL "
            "reason=NOT_SECURED_PACKET "
            "next_header="
            << static_cast<unsigned>(nextHeader)
            << " name="
            << NextHeaderBasicToString(
                   nextHeader));

        return false;
    }

    const std::vector<uint8_t> securedBytes(
        networkBytes.begin() + basicHeaderSize,
        networkBytes.end());

    if (securedBytes.empty())
    {
        NS_LOG_UNCOND(
            "[SECURED_TX] RESULT=FAIL "
            "reason=EMPTY_SECURED_MESSAGE");

        return false;
    }

    const std::string serialized(
        reinterpret_cast<const char*>(
            securedBytes.data()),
        securedBytes.size());

    std::istringstream input(
        serialized,
        std::ios::binary);

    vanetza::InputArchive archive(input);

    vanetza::security::v3::SecuredMessage securedMessage;

    std::size_t consumed = 0;

    try
    {
        consumed =
            vanetza::security::v3::deserialize(
                archive,
                securedMessage);
    }
    catch (const std::exception& e)
    {
        NS_LOG_UNCOND(
            "[SECURED_TX] RESULT=FAIL "
            "reason=DESERIALIZATION_EXCEPTION "
            "error="
            << e.what());

        return false;
    }
    catch (...)
    {
        NS_LOG_UNCOND(
            "[SECURED_TX] RESULT=FAIL "
            "reason=DESERIALIZATION_EXCEPTION "
            "error=UNKNOWN");

        return false;
    }

    if (!archive.is_good())
    {
        NS_LOG_UNCOND(
            "[SECURED_TX] RESULT=FAIL "
            "reason=DESERIALIZATION_ARCHIVE_ERROR "
            "remaining="
            << archive.remaining_bytes());

        return false;
    }

    const vanetza::ItsAid actualAid =
        securedMessage.its_aid();

    const std::size_t securedSize =
        securedBytes.size();

    const std::size_t remaining =
        archive.remaining_bytes();

    const bool completeMessageConsumed =
        remaining == 0;

    const bool aidOk =
        actualAid != 0;

    NS_LOG_UNCOND(
        "[SECURED_TX] "
        "version="
        << static_cast<unsigned>(version)

        << " next_header="
        << static_cast<unsigned>(nextHeader)

        << " network_size="
        << networkBytes.size()

        << " secured_size="
        << securedSize

        << " consumed="
        << consumed

        << " remaining="
        << remaining

        << " its_aid="
        << static_cast<unsigned>(actualAid)

        << " expected_its_aid=ANY_SUPPORTED"

        << " aid_ok="
        << (aidOk ? "YES" : "NO")

        << " complete="
        << (completeMessageConsumed ? "YES" : "NO")

        << " RESULT="
        << ((aidOk &&
             completeMessageConsumed)
                ? "PASS"
                : "FAIL"));

    return aidOk &&
           completeMessageConsumed;
}


bool
Ns3AccessAdapter::ValidateCommonHeaderTx(
    const std::vector<uint8_t>& networkBytes,
    std::size_t transportSize,
    std::size_t applicationSize,
    std::size_t frameSize) const
{
    constexpr std::size_t basicHeaderSize = 4;
    constexpr std::size_t commonHeaderSize = 8;

    const bool securedPacket =
        m_mib.itsGnSecurity;

    vanetza::ByteBuffer securedPayload;
    vanetza::ItsAid securedAid = 0;

    if (securedPacket)
    {
        if (!ExtractSecuredV3GeoNetworkingPayload(
                networkBytes,
                securedPayload,
                securedAid))
        {
            NS_LOG_UNCOND(
                "[GEONET_COMMON_TX] RESULT=FAIL "
                "reason=SECURED_PAYLOAD_EXTRACTION_FAILED");

            return false;
        }
    }

    const std::vector<uint8_t> securedPayloadVector(
        securedPayload.begin(),
        securedPayload.end());

    const std::vector<uint8_t>& bytes =
        securedPacket
            ? securedPayloadVector
            : networkBytes;

    const std::size_t commonHeaderOffset =
        securedPacket
            ? 0
            : basicHeaderSize;

    const std::size_t requiredSize =
        commonHeaderOffset +
        commonHeaderSize;

    if (bytes.size() < requiredSize)
    {
        NS_LOG_UNCOND(
            "[GEONET_COMMON_TX] RESULT=FAIL "
            "reason=NETWORK_LAYER_TOO_SHORT "
            "secured="
            << (securedPacket ? "YES" : "NO")
            << " aid="
            << static_cast<unsigned>(
                   securedAid)
            << " size="
            << bytes.size()
            << " required="
            << requiredSize);

        return false;
    }

    const std::size_t offset =
        commonHeaderOffset;

    const uint8_t octet0 =
        bytes[offset + 0];

    const uint8_t nextHeader =
        (octet0 >> 4) & 0x0F;

    const uint8_t reserved1 =
        octet0 & 0x0F;

    const uint8_t headerType =
        bytes[offset + 1];

    const uint8_t trafficClass =
        bytes[offset + 2];

    const uint8_t flags =
        bytes[offset + 3];

    const uint16_t payloadLength =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(
                 bytes[offset + 4]) << 8) |
            static_cast<uint16_t>(
                 bytes[offset + 5]));

    const uint8_t maximumHopLimit =
        bytes[offset + 6];

    const uint8_t reserved2 =
        bytes[offset + 7];

    const bool isBeacon =
        headerType == 0x10;

    const bool isTsbSingleHop =
        headerType == 0x50;

    const bool isGeoBroadcastCircle =
        headerType == 0x40;

    const uint8_t expectedNextHeader =
        isBeacon
            ? 0
            : ((isTsbSingleHop || isGeoBroadcastCircle) ? 2 : 0);

    const uint8_t expectedHeaderType =
        isBeacon
            ? 0x10
            : (isTsbSingleHop
                   ? 0x50
                   : (isGeoBroadcastCircle ? 0x40 : 0x00));

    const uint8_t expectedTrafficClass =
        0x00;

    const uint8_t expectedFlags =
        m_mib.itsGnIsMobile
            ? 0x80
            : 0x00;

    const uint8_t expectedMaximumHopLimit =
        isGeoBroadcastCircle
            ? 10
            : 1;

    bool payloadLengthOk = false;
    std::size_t expectedPayloadLength = 0;

    if (securedPacket)
    {
        if (isBeacon)
        {
            expectedPayloadLength = 0;

            payloadLengthOk =
                payloadLength == 0;
        }
        else if (isTsbSingleHop)
        {
            constexpr std::size_t btpBHeaderSize = 4;

            expectedPayloadLength =
                btpBHeaderSize;

            payloadLengthOk =
                payloadLength >= btpBHeaderSize;
        }
        else
        {
            expectedPayloadLength = 0;
            payloadLengthOk = true;
        }
    }
    else
    {
        expectedPayloadLength =
            transportSize +
            applicationSize;

        payloadLengthOk =
            payloadLength ==
            expectedPayloadLength;
    }

    std::size_t expectedFrameSize = 0;
    bool frameSizeOk = false;

    if (securedPacket)
    {
        expectedFrameSize =
            networkBytes.size();

        frameSizeOk =
            frameSize ==
            expectedFrameSize;
    }
    else
    {
        expectedFrameSize =
            networkBytes.size() +
            expectedPayloadLength;

        frameSizeOk =
            frameSize ==
            expectedFrameSize;
    }

    const bool nextHeaderOk =
        nextHeader ==
        expectedNextHeader;

    const bool reserved1Ok =
        reserved1 == 0;

    const bool headerTypeOk =
        headerType ==
        expectedHeaderType;

    const bool trafficClassOk =
        trafficClass ==
        expectedTrafficClass;

    const bool flagsOk =
        flags ==
        expectedFlags;

    const bool maximumHopLimitOk =
        maximumHopLimit ==
        expectedMaximumHopLimit;

    const bool reserved2Ok =
        reserved2 == 0;

    const bool packetTypeOk =
        isBeacon ||
        isTsbSingleHop ||
        isGeoBroadcastCircle;

    const bool beaconPayloadOk =
        !isBeacon ||
        payloadLength == 0;

    bool securedStructureOk =
        true;

    if (securedPacket &&
        isTsbSingleHop)
    {
        constexpr std::size_t shbHeaderSize = 28;
        constexpr std::size_t btpBHeaderSize = 4;

        const std::size_t payloadOffset =
            offset +
            commonHeaderSize;

        const std::size_t minimumAfterCommon =
            shbHeaderSize +
            btpBHeaderSize;

        securedStructureOk =
            bytes.size() >=
            payloadOffset +
            minimumAfterCommon;
    }

    const bool result =
        nextHeaderOk &&
        reserved1Ok &&
        headerTypeOk &&
        trafficClassOk &&
        flagsOk &&
        payloadLengthOk &&
        maximumHopLimitOk &&
        reserved2Ok &&
        packetTypeOk &&
        beaconPayloadOk &&
        frameSizeOk &&
        securedStructureOk;

    std::ostringstream oss;

    oss << "[GEONET_COMMON_TX]"
        << " secured="
        << (securedPacket ? "YES" : "NO")

        << " aid="
        << static_cast<unsigned>(
               securedAid)

        << " common_offset="
        << offset

        << " next_header="
        << static_cast<unsigned>(
               nextHeader)

        << "("
        << CommonNextHeaderToString(
               nextHeader)
        << ")"

        << " expected="
        << static_cast<unsigned>(
               expectedNextHeader)

        << " ok="
        << (nextHeaderOk
                ? "YES"
                : "NO")

        << " reserved1="
        << static_cast<unsigned>(
               reserved1)

        << " ok="
        << (reserved1Ok
                ? "YES"
                : "NO")

        << " header_type=0x"
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<unsigned>(
               headerType)

        << std::dec

        << " name="
        << GeoNetworkingHeaderTypeToString(
               headerType)

        << " expected=0x"
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<unsigned>(
               expectedHeaderType)

        << std::dec

        << " ok="
        << (headerTypeOk
                ? "YES"
                : "NO")

        << " traffic_class="
        << static_cast<unsigned>(
               trafficClass)

        << " expected="
        << static_cast<unsigned>(
               expectedTrafficClass)

        << " ok="
        << (trafficClassOk
                ? "YES"
                : "NO")

        << " flags="
        << static_cast<unsigned>(
               flags)

        << " expected="
        << static_cast<unsigned>(
               expectedFlags)

        << " ok="
        << (flagsOk
                ? "YES"
                : "NO")

        << " payload_length="
        << payloadLength

        << " minimum_expected="
        << expectedPayloadLength

        << " transport_outer="
        << transportSize

        << " application_outer="
        << applicationSize

        << " payload_ok="
        << (payloadLengthOk
                ? "YES"
                : "NO")

        << " maximum_hop_limit="
        << static_cast<unsigned>(
               maximumHopLimit)

        << " expected="
        << static_cast<unsigned>(
               expectedMaximumHopLimit)

        << " ok="
        << (maximumHopLimitOk
                ? "YES"
                : "NO")

        << " reserved2="
        << static_cast<unsigned>(
               reserved2)

        << " ok="
        << (reserved2Ok
                ? "YES"
                : "NO")

        << " packet_type="
        << (isBeacon
        ? "BEACON"
        : (isTsbSingleHop
            ? "TSB_SINGLE_HOP"
            : (isGeoBroadcastCircle
                ? "GEO_BROADCAST_CIRCLE"
                : "UNKNOWN")))

        << " packet_type_ok="
        << (packetTypeOk
                ? "YES"
                : "NO")

        << " secured_structure_ok="
        << (securedStructureOk
                ? "YES"
                : "NO")

        << " network_size="
        << networkBytes.size()

        << " geo_payload_size="
        << bytes.size()

        << " expected_frame_size="
        << expectedFrameSize

        << " actual_frame_size="
        << frameSize

        << " frame_size_ok="
        << (frameSizeOk
                ? "YES"
                : "NO")

        << " raw=";

    for (std::size_t i = offset;
         i < offset + commonHeaderSize;
         ++i)
    {
        if (i != offset)
        {
            oss << " ";
        }

        oss << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(
                   bytes[i]);
    }

    oss << std::dec
        << " RESULT="
        << (result
                ? "PASS"
                : "FAIL");

    NS_LOG_UNCOND(oss.str());

    return result;
}


bool
Ns3AccessAdapter::ValidateBasicHeaderTx(
    const std::vector<uint8_t>& networkBytes) const
{
    if (networkBytes.size() < 4)
    {
        NS_LOG_UNCOND(
            "[GEONET_BASIC_TX] RESULT=FAIL "
            "reason=NETWORK_LAYER_TOO_SHORT "
            "size="
            << networkBytes.size());

        return false;
    }

    const uint8_t octet0 =
        networkBytes[0];

    const uint8_t version =
        (octet0 >> 4) & 0x0F;

    const uint8_t nextHeader =
        octet0 & 0x0F;

    const uint8_t reserved =
        networkBytes[1];

    const uint8_t lifetimeRaw =
        networkBytes[2];

    const uint8_t remainingHopLimit =
        networkBytes[3];

    const uint8_t expectedVersion =
        static_cast<uint8_t>(
            m_mib.itsGnProtocolVersion);

    const uint8_t expectedNextHeader =
        m_mib.itsGnSecurity
            ? 2
            : 1;

    const uint8_t lifetimeBase =
        lifetimeRaw & 0x03;

    const uint8_t lifetimeMultiplier =
        lifetimeRaw >> 2;

    double lifetimeSeconds = 0.0;

    switch (lifetimeBase)
    {
        case 0:
            lifetimeSeconds =
                static_cast<double>(
                    lifetimeMultiplier) *
                0.05;
            break;

        case 1:
            lifetimeSeconds =
                static_cast<double>(
                    lifetimeMultiplier);
            break;

        case 2:
            lifetimeSeconds =
                static_cast<double>(
                    lifetimeMultiplier) *
                10.0;
            break;

        case 3:
            lifetimeSeconds =
                static_cast<double>(
                    lifetimeMultiplier) *
                100.0;
            break;

        default:
            lifetimeSeconds =
                -1.0;
            break;
    }

    const bool versionOk =
        version ==
        expectedVersion;

    const bool nextHeaderOk =
        nextHeader ==
        expectedNextHeader;

    const bool reservedOk =
        reserved == 0;

    const bool lifetimeOk =
        lifetimeSeconds >= 0.0;

    const uint8_t minimumRemainingHopLimit =
        1;

    const bool remainingHopLimitOk =
        remainingHopLimit >=
        minimumRemainingHopLimit;

    const bool result =
        versionOk &&
        nextHeaderOk &&
        reservedOk &&
        lifetimeOk &&
        remainingHopLimitOk;

    NS_LOG_UNCOND(
        "[GEONET_BASIC_TX] "
        "version="
        << static_cast<unsigned>(
               version)

        << " expected="
        << static_cast<unsigned>(
               expectedVersion)

        << " ok="
        << (versionOk ? "YES" : "NO")

        << " next_header="
        << static_cast<unsigned>(
               nextHeader)

        << "("
        << NextHeaderBasicToString(
               nextHeader)
        << ")"

        << " expected="
        << static_cast<unsigned>(
               expectedNextHeader)

        << " ok="
        << (nextHeaderOk ? "YES" : "NO")

        << " reserved="
        << static_cast<unsigned>(
               reserved)

        << " ok="
        << (reservedOk ? "YES" : "NO")

        << " lifetime_raw="
        << static_cast<unsigned>(
               lifetimeRaw)

        << " lifetime_base="
        << static_cast<unsigned>(
               lifetimeBase)

        << " lifetime_multiplier="
        << static_cast<unsigned>(
               lifetimeMultiplier)

        << " lifetime_seconds="
        << lifetimeSeconds

        << " ok="
        << (lifetimeOk ? "YES" : "NO")

        << " remaining_hop_limit="
        << static_cast<unsigned>(
               remainingHopLimit)

        << " minimum="
        << static_cast<unsigned>(
               minimumRemainingHopLimit)

        << " ok="
        << (remainingHopLimitOk
                ? "YES"
                : "NO")

        << " raw_octet=0x"
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<unsigned>(
               networkBytes[3])
        << std::dec

        << " raw="
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<unsigned>(
               networkBytes[0])
        << " "
        << std::setw(2)
        << static_cast<unsigned>(
               networkBytes[1])
        << " "
        << std::setw(2)
        << static_cast<unsigned>(
               networkBytes[2])
        << " "
        << std::setw(2)
        << static_cast<unsigned>(
               networkBytes[3])
        << std::dec

        << " RESULT="
        << (result ? "PASS" : "FAIL"));

    return result;
}


bool
Ns3AccessAdapter::ValidateShbExtendedHeaderTx(
    const std::vector<uint8_t>& networkBytes) const
{
    constexpr std::size_t basicHeaderSize = 4;
    constexpr std::size_t commonHeaderSize = 8;
    constexpr std::size_t shbHeaderSize = 28;
    constexpr std::size_t longPositionVectorSize = 24;

    const bool securedPacket =
        m_mib.itsGnSecurity;

    vanetza::ByteBuffer securedPayload;
    vanetza::ItsAid securedAid = 0;

    if (securedPacket)
    {
        if (!ExtractSecuredV3GeoNetworkingPayload(
                networkBytes,
                securedPayload,
                securedAid))
        {
            NS_LOG_UNCOND(
                "[GEONET_SHB_TX] RESULT=FAIL "
                "reason=SECURED_PAYLOAD_EXTRACTION_FAILED");

            return false;
        }
    }

    const std::vector<uint8_t> securedPayloadVector(
        securedPayload.begin(),
        securedPayload.end());

    const std::vector<uint8_t>& bytes =
        securedPacket
            ? securedPayloadVector
            : networkBytes;

    const std::size_t commonHeaderOffset =
        securedPacket
            ? 0
            : basicHeaderSize;

    const std::size_t shbOffset =
        commonHeaderOffset +
        commonHeaderSize;

    const std::size_t dccOffset =
        shbOffset +
        longPositionVectorSize;

    const std::size_t minimumNetworkSize =
        shbOffset +
        shbHeaderSize;

    if (bytes.size() <
        minimumNetworkSize)
    {
        NS_LOG_UNCOND(
            "[GEONET_SHB_TX] RESULT=FAIL "
            "reason=NETWORK_LAYER_TOO_SHORT "
            "secured="
            << (securedPacket ? "YES" : "NO")
            << " aid="
            << static_cast<unsigned>(
                   securedAid)
            << " network_layer_size="
            << bytes.size()
            << " expected_min="
            << minimumNetworkSize);

        return false;
    }

    const uint8_t headerType =
        bytes[commonHeaderOffset + 1];

    const bool shbTypeOk =
        headerType == 0x50;

    if (!shbTypeOk)
    {
        NS_LOG_UNCOND(
            "[GEONET_SHB_TX] RESULT=FAIL "
            "reason=NOT_TSB_SINGLE_HOP "
            "header_type=0x"
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(
                   headerType)
            << std::dec
            << " name="
            << GeoNetworkingHeaderTypeToString(
                   headerType));

        return false;
    }

    const std::size_t pvOffset =
        shbOffset;

    const uint16_t addressControl =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(
                 bytes[pvOffset + 0]) << 8) |
            static_cast<uint16_t>(
                 bytes[pvOffset + 1]));

    const bool manuallyConfigured =
        (addressControl & 0x8000) != 0;

    const uint8_t stationType =
        static_cast<uint8_t>(
            (addressControl & 0x7C00) >> 10);

    const uint16_t countryCode =
        addressControl & 0x03FF;

    std::ostringstream mid;

    mid << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<unsigned>(
               bytes[pvOffset + 2])
        << ":"
        << std::setw(2)
        << static_cast<unsigned>(
               bytes[pvOffset + 3])
        << ":"
        << std::setw(2)
        << static_cast<unsigned>(
               bytes[pvOffset + 4])
        << ":"
        << std::setw(2)
        << static_cast<unsigned>(
               bytes[pvOffset + 5])
        << ":"
        << std::setw(2)
        << static_cast<unsigned>(
               bytes[pvOffset + 6])
        << ":"
        << std::setw(2)
        << static_cast<unsigned>(
               bytes[pvOffset + 7])
        << std::dec;

    const uint32_t timestamp =
        (static_cast<uint32_t>(
             bytes[pvOffset + 8]) << 24) |
        (static_cast<uint32_t>(
             bytes[pvOffset + 9]) << 16) |
        (static_cast<uint32_t>(
             bytes[pvOffset + 10]) << 8) |
        static_cast<uint32_t>(
            bytes[pvOffset + 11]);

    const auto& localPositionVector =
        m_router.get_local_position_vector();

    const uint32_t routerTimestamp =
        localPositionVector.timestamp.raw();

    const bool timestampNonZero =
        timestamp != 0;

    const bool timestampOk =
        timestampNonZero &&
        timestamp == routerTimestamp;

    const int64_t timestampDeltaMs =
        static_cast<int64_t>(
            timestamp) -
        static_cast<int64_t>(
            routerTimestamp);

    const int32_t latitudeRaw =
        static_cast<int32_t>(
            (static_cast<uint32_t>(
                 bytes[pvOffset + 12]) << 24) |
            (static_cast<uint32_t>(
                 bytes[pvOffset + 13]) << 16) |
            (static_cast<uint32_t>(
                 bytes[pvOffset + 14]) << 8) |
            static_cast<uint32_t>(
                bytes[pvOffset + 15]));

    const int32_t longitudeRaw =
        static_cast<int32_t>(
            (static_cast<uint32_t>(
                 bytes[pvOffset + 16]) << 24) |
            (static_cast<uint32_t>(
                 bytes[pvOffset + 17]) << 16) |
            (static_cast<uint32_t>(
                 bytes[pvOffset + 18]) << 8) |
            static_cast<uint32_t>(
                bytes[pvOffset + 19]));

    const double latitudeDegrees =
        static_cast<double>(
            latitudeRaw) /
        10000000.0;

    const double longitudeDegrees =
        static_cast<double>(
            longitudeRaw) /
        10000000.0;

    const uint16_t paiAndSpeed =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(
                 bytes[pvOffset + 20]) << 8) |
            static_cast<uint16_t>(
                 bytes[pvOffset + 21]));

    const bool pai =
        (paiAndSpeed & 0x8000) != 0;

    const uint16_t speedRaw =
        paiAndSpeed & 0x7FFF;

    const double speedMps =
        static_cast<double>(
            speedRaw) /
        100.0;

    const uint16_t headingRaw =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(
                 bytes[pvOffset + 22]) << 8) |
            static_cast<uint16_t>(
                bytes[pvOffset + 23]));

    const double headingDegrees =
        static_cast<double>(
            headingRaw) *
        0.1;

    const uint32_t dccRaw =
        (static_cast<uint32_t>(
             bytes[dccOffset + 0]) << 24) |
        (static_cast<uint32_t>(
             bytes[dccOffset + 1]) << 16) |
        (static_cast<uint32_t>(
             bytes[dccOffset + 2]) << 8) |
        static_cast<uint32_t>(
            bytes[dccOffset + 3]);

    const bool sizeOk =
        bytes.size() >=
        minimumNetworkSize;

    const bool latitudeOk =
        latitudeDegrees >= -90.0 &&
        latitudeDegrees <= 90.0;

    const bool longitudeOk =
        longitudeDegrees >= -180.0 &&
        longitudeDegrees <= 180.0;

    const bool headingOk =
        headingRaw <= 3600;

    const bool addressOk =
        manuallyConfigured;

    const bool structureOk =
        sizeOk &&
        shbTypeOk &&
        latitudeOk &&
        longitudeOk &&
        headingOk &&
        addressOk &&
        timestampOk;

    std::ostringstream raw;

    raw << std::hex
        << std::setfill('0');

    for (std::size_t i = 0;
         i < shbHeaderSize;
         ++i)
    {
        if (i != 0)
        {
            raw << " ";
        }

        raw << std::setw(2)
            << static_cast<unsigned>(
                   bytes[
                       shbOffset + i]);
    }

    std::ostringstream log;

    log << "[GEONET_SHB_TX] "
        << "secured="
        << (securedPacket ? "YES" : "NO")

        << " aid="
        << static_cast<unsigned>(
               securedAid)

        << " common_offset="
        << commonHeaderOffset

        << " shb_offset="
        << shbOffset

        << " size="
        << shbHeaderSize

        << " expected="
        << shbHeaderSize

        << " source_mid="
        << mid.str()

        << " manually_configured="
        << (manuallyConfigured ? "YES" : "NO")

        << " timestamp_raw="
        << timestamp

        << " router_timestamp="
        << routerTimestamp

        << " timestamp_delta_ms="
        << timestampDeltaMs

        << " timestamp_ok="
        << (timestampOk ? "YES" : "NO")

        << " station_type="
        << static_cast<unsigned>(
               stationType)

        << " country_code="
        << countryCode

        << " timestamp="
        << timestamp

        << " latitude_raw="
        << latitudeRaw

        << " latitude_deg="
        << latitudeDegrees

        << " longitude_raw="
        << longitudeRaw

        << " longitude_deg="
        << longitudeDegrees

        << " pai="
        << (pai ? "1" : "0")

        << " speed_raw="
        << speedRaw

        << " speed_mps="
        << speedMps

        << " heading_raw="
        << headingRaw

        << " heading_deg="
        << headingDegrees

        << " dcc_raw=0x"
        << std::hex
        << std::setw(8)
        << std::setfill('0')
        << dccRaw
        << std::dec

        << " address_ok="
        << (addressOk ? "YES" : "NO")

        << " latitude_ok="
        << (latitudeOk ? "YES" : "NO")

        << " longitude_ok="
        << (longitudeOk ? "YES" : "NO")

        << " heading_ok="
        << (headingOk ? "YES" : "NO")

        << " structure_ok="
        << (structureOk ? "YES" : "NO")

        << " raw="
        << raw.str()

        << " RESULT="
        << (structureOk ? "PASS" : "FAIL");

    NS_LOG_UNCOND(log.str());

    return structureOk;
}


Ns3AccessAdapter::Ns3AccessAdapter(
    VehicleState& vehicle,
    const vanetza::geonet::MIB& mib,
    vanetza::geonet::Router& router)
    :
    m_node(vehicle.node),
    m_socket(vehicle.socket),
    m_mib(mib),
    m_router(router)
{
}

Ns3AccessAdapter::Ns3AccessAdapter(
    ItsStationState& station,
    const vanetza::geonet::MIB& mib,
    vanetza::geonet::Router& router)
    :
    m_node(station.node),
    m_socket(station.socket),
    m_mib(mib),
    m_router(router)
{
}


std::vector<uint8_t>
Ns3AccessAdapter::SerializePacket(
    const vanetza::ChunkPacket& packet) const
{
    std::vector<uint8_t> bytes;

    NS_LOG_UNCOND(
        "[TX_BYTELEVEL] =================================================");

    NS_LOG_UNCOND(
        "[TX_BYTELEVEL] BEGIN ChunkPacket -> ns3 frame");

    for (int layer =
             static_cast<int>(
                 vanetza::OsiLayer::Physical);
         layer <=
             static_cast<int>(
                 vanetza::OsiLayer::Application);
         ++layer)
    {
        const auto osiLayer =
            static_cast<vanetza::OsiLayer>(
                layer);

        vanetza::ByteBuffer buffer;

        packet.layer(osiLayer)
            .convert(buffer);

        const std::size_t offset =
            bytes.size();

        const std::size_t size =
            buffer.size();

        std::vector<uint8_t> layerBytes(
            buffer.begin(),
            buffer.end());

        NS_LOG_UNCOND(
            "[TX_BYTELEVEL] layer="
            << LayerName(osiLayer)
            << " offset="
            << offset
            << " size="
            << size);

        if (!layerBytes.empty())
        {
            NS_LOG_UNCOND(
                "[TX_BYTELEVEL] "
                << LayerName(osiLayer)
                << "_HEX_BEGIN");

            NS_LOG_UNCOND(
                HexDump(layerBytes));

            NS_LOG_UNCOND(
                "[TX_BYTELEVEL] "
                << LayerName(osiLayer)
                << "_HEX_END");
        }

        bytes.insert(
            bytes.end(),
            layerBytes.begin(),
            layerBytes.end());
    }

    NS_LOG_UNCOND(
        "[TX_BYTELEVEL] FINAL_FRAME_SIZE="
        << bytes.size());

    NS_LOG_UNCOND(
        "[TX_BYTELEVEL] FINAL_FRAME_HEX_BEGIN");

    NS_LOG_UNCOND(
        HexDump(bytes));

    NS_LOG_UNCOND(
        "[TX_BYTELEVEL] FINAL_FRAME_HEX_END");

    NS_LOG_UNCOND(
        "[TX_BYTELEVEL] =================================================");

    return bytes;
}


void
Ns3AccessAdapter::request(
    const vanetza::access::DataRequest&,
    std::unique_ptr<vanetza::ChunkPacket> packet)
{
    std::shared_ptr<PacketContext> ctx = TxBridgeContext::Current();


    TxBridgeContext::Pop();

    //----------------------------------------------------------------------
    // Validation
    //----------------------------------------------------------------------

    if (!packet)
    {
        if (ctx)
        {
            ctx->AdvancePipeline(
                PipelineStage::ACCESS_TX);

            ctx->MarkFailure();

            ctx->reason =
                "null_packet";

            V2XWriteLog(
                *ctx,
                V2XLogLevel::ERROR,
                "ACCESS",
                "ACCESS_TX_FAIL",
                "Null ChunkPacket");

            TxBridgeContext::Pop();
        }

        return;
    }

    if (!m_socket)
    {
        if (ctx)
        {
            ctx->AdvancePipeline(
                PipelineStage::ACCESS_TX);

            ctx->MarkFailure();

            ctx->reason =
                "socket_unavailable";

            V2XWriteLog(
                *ctx,
                V2XLogLevel::ERROR,
                "ACCESS",
                "ACCESS_TX_FAIL",
                "Socket unavailable");

            TxBridgeContext::Pop();
        }

        return;
    }

    //----------------------------------------------------------------------
    // Serialize complete Down/ChunkPacket
    //----------------------------------------------------------------------

    std::vector<uint8_t> frame =
        SerializePacket(*packet);

    //----------------------------------------------------------------------
    // GeoNetworking Network Layer
    //----------------------------------------------------------------------

    vanetza::ByteBuffer networkBuffer;

    packet->layer(
        vanetza::OsiLayer::Network)
        .convert(networkBuffer);

    std::vector<uint8_t> networkBytes(
        networkBuffer.begin(),
        networkBuffer.end());

    NS_LOG_UNCOND(
        "[GEONET_BASIC_TX] network_layer_size="
        << networkBytes.size());

    if (!networkBytes.empty())
    {
        NS_LOG_UNCOND(
            "[GEONET_BASIC_TX] NETWORK_HEX_BEGIN");

        NS_LOG_UNCOND(
            HexDump(networkBytes));

        NS_LOG_UNCOND(
            "[GEONET_BASIC_TX] NETWORK_HEX_END");
    }

    ValidateBasicHeaderTx(
        networkBytes);

    vanetza::ItsAid securedTxAid = 0;
    bool securedTxAidValid = false;

    if (m_mib.itsGnSecurity)
    {
        //------------------------------------------------------------------
        // Secured Packet
        //
        // The security envelope is validated first.
        // Afterwards the actual GeoNetworking payload contained inside
        // the SecuredMessage determines whether the packet is a Beacon
        // or TSB Single Hop.
        //------------------------------------------------------------------

        ValidateSecuredMessageV3Tx(
            networkBytes);

        vanetza::ByteBuffer securedPayload;

        vanetza::ItsAid securedAid = 0;

        if (!ExtractSecuredV3GeoNetworkingPayload(
                networkBytes,
                securedPayload,
                securedAid))
        {
            NS_LOG_UNCOND(
                "[GEONET_TX] RESULT=FAIL "
                "reason=SECURED_GEO_PAYLOAD_EXTRACTION_FAILED");

        }
        else
        {
            securedTxAid = securedAid;
            securedTxAidValid = true;

            vanetza::geonet::Parser payloadParser(
                securedPayload.begin(),
                securedPayload.end());

            vanetza::geonet::CommonHeader common;

            if (payloadParser.parse_common(common) == 0)
            {
                NS_LOG_UNCOND(
                    "[GEONET_TX] RESULT=FAIL "
                    "reason=SECURED_GEO_PAYLOAD_COMMON_PARSE_FAILED "
                    "its_aid="
                    << static_cast<unsigned>(
                           securedAid));
            }
            else
            {
                const uint8_t headerType =
                    static_cast<uint8_t>(
                        common.header_type);

                NS_LOG_UNCOND(
                    "[GEONET_TX] "
                    "its_aid="
                    << static_cast<unsigned>(
                           securedAid)
                    << " header_type=0x"
                    << std::hex
                    << std::setw(2)
                    << std::setfill('0')
                    << static_cast<unsigned>(
                           headerType)
                    << std::dec
                    << " header_name="
                    << GeoNetworkingHeaderTypeToString(
                           headerType));

                if (common.header_type ==
                    vanetza::geonet::HeaderType::Beacon)
                {
                    //------------------------------------------------------------------
                    // GN_MGMT / Beacon
                    //------------------------------------------------------------------

                    ValidateCommonHeaderTx(
                        networkBytes,
                        0,
                        0,
                        frame.size());

                    ValidateBeaconPayloadTx(
                        securedPayload);
                }
                else if (common.header_type ==
                         vanetza::geonet::HeaderType::TSB_Single_Hop)
                {
                    //------------------------------------------------------------------
                    // CA / TSB Single Hop / SHB / BTP-B / CAM
                    //------------------------------------------------------------------

                    ValidateCommonHeaderTx(
                        networkBytes,
                        0,
                        0,
                        frame.size());

                    ValidateShbExtendedHeaderTx(
                        networkBytes);
                }
                else if (common.header_type ==
                         vanetza::geonet::HeaderType::GeoBroadcast_Circle)
                {
                    //------------------------------------------------------------------
                    // DENM / GeoBroadcast Circle / GBC
                    //
                    // GBC does not use the SHB/TSB extended header validator.
                    // Validate the common GeoNetworking header only here.
                    //------------------------------------------------------------------

                    ValidateCommonHeaderTx(
                        networkBytes,
                        0,
                        0,
                        frame.size());
                }
                else
                {
                    NS_LOG_UNCOND(
                        "[GEONET_TX] "
                        "RESULT=FAIL "
                        "reason=UNSUPPORTED_SECURED_GEONET_HEADER_TYPE "
                        "its_aid="
                        << static_cast<unsigned>(
                               securedAid)
                        << " header_type=0x"
                        << std::hex
                        << std::setw(2)
                        << std::setfill('0')
                        << static_cast<unsigned>(
                               headerType)
                        << std::dec
                        << " header_name="
                        << GeoNetworkingHeaderTypeToString(
                               headerType));
                }
            }
        }
    }
    else
    {
        //------------------------------------------------------------------
        // Normal GeoNetworking Packet
        //------------------------------------------------------------------

        vanetza::ByteBuffer transportBuffer;

        packet->layer(
            vanetza::OsiLayer::Transport)
            .convert(transportBuffer);

        const std::size_t transportSize =
            transportBuffer.size();

        vanetza::ByteBuffer applicationBuffer;

        packet->layer(
            vanetza::OsiLayer::Application)
            .convert(applicationBuffer);

        const std::size_t applicationSize =
            applicationBuffer.size();

        ValidateCommonHeaderTx(
            networkBytes,
            transportSize,
            applicationSize,
            frame.size());

        const uint8_t headerType =
            (networkBytes.size() >= 6)
                ? networkBytes[5]
                : 0x00;

        if (headerType == 0x50)
        {
            ValidateShbExtendedHeaderTx(
                networkBytes);
        }
    }

    if (frame.empty())
    {
        if (ctx)
        {
            ctx->AdvancePipeline(
                PipelineStage::ACCESS_TX);

            ctx->MarkFailure();

            ctx->reason =
                "empty_frame";

            V2XWriteLog(
                *ctx,
                V2XLogLevel::ERROR,
                "ACCESS",
                "ACCESS_TX_FAIL",
                "Serialized frame is empty");

            TxBridgeContext::Pop();
        }

        return;
    }

    //----------------------------------------------------------------------
    // TEST HOOK: deterministic DENM V3 wire tampering.
    //
    // Applied only after the complete secured frame has been serialized
    // and all TX structural validation has already succeeded.
    //
    // The target is the final byte of the serialized Network layer.
    // With the current V3 layout this byte is inside the SecuredMessage
    // signature field. The actual ns-3 Packet is created only afterwards,
    // so this mutation affects the bytes put on the simulated wire.
    //
    // Normal behavior is unchanged unless:
    //     V2X_DENM_TAMPER=1
    //----------------------------------------------------------------------

    const char* tamperEnv =
        std::getenv("V2X_DENM_TAMPER");

    if (securedTxAidValid &&
        securedTxAid == 37 &&
        tamperEnv != nullptr &&
        std::string(tamperEnv) == "1" &&
        !frame.empty() &&
        !networkBytes.empty() &&
        networkBytes.size() <= frame.size())
    {
        const std::size_t networkOffset =
            frame.size() - networkBytes.size();

        const std::size_t tamperOffset =
            networkOffset + networkBytes.size() - 1;

        const uint8_t originalByte =
            frame[tamperOffset];

        frame[tamperOffset] ^= 0x01;

        NS_LOG_UNCOND(
            "[DENM_V3_TAMPER] "
            "aid=37 "
            "target=network_last_byte "
            "offset="
            << tamperOffset
            << " original=0x"
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(
                   originalByte)
            << " tampered=0x"
            << std::setw(2)
            << static_cast<unsigned>(
                   frame[tamperOffset])
            << std::dec
            << " frame_size="
            << frame.size()
            << " network_size="
            << networkBytes.size());
    }

    //----------------------------------------------------------------------
    // STEP 6 : ACCESS_TX
    //----------------------------------------------------------------------

    if (ctx)
    {
        ctx->AdvancePipeline(
            PipelineStage::ACCESS_TX);

        ctx->payload.bytes =
            frame;

        V2XWriteLog(
            *ctx,
            V2XLogLevel::DEBUG,
            "ACCESS",
            "ACCESS_TX_BEGIN",
            "Serialized complete Vanetza frame");

        ctx->MarkSuccess();

        V2XWriteLog(
            *ctx,
            V2XLogLevel::DEBUG,
            "ACCESS",
            "ACCESS_TX_OK",
            "bytes="
            + std::to_string(
                  frame.size()));
    }

    //----------------------------------------------------------------------
    // Convert vector<uint8_t> -> ns3::Packet
    //----------------------------------------------------------------------

    Ptr<Packet> ns3Packet =
        Create<Packet>(
            frame.data(),
            frame.size());

    if (ctx && ctx->packetId != 0)
    {
        PacketIdTag tag(ctx->packetId);
        ns3Packet->AddPacketTag(tag);

        NS_LOG_UNCOND(
            "[TX_PACKET_ID_TAG] packet_id="
            << ctx->packetId);
    }

    NS_LOG_UNCOND(
        "[TX_BYTELEVEL] ns3::Packet created size="
        << ns3Packet->GetSize());

    NS_ASSERT_MSG(
        ns3Packet->GetSize() ==
            frame.size(),
        "ns3::Packet size differs from serialized frame");

    Ptr<Socket> socket =
        m_socket;

    //----------------------------------------------------------------------
    // TEST HOOK: deterministic DENM V3 duplicate.
    //
    // Reuses an exact copy of the already serialized ns3::Packet so that
    // the simulated wire receives the same secured DENM twice.
    //
    // Normal behavior is unchanged unless:
    //     V2X_DENM_DUPLICATE=1
    //----------------------------------------------------------------------

    const char* duplicateEnv =
        std::getenv("V2X_DENM_DUPLICATE");

    const bool duplicateDenm =
        ctx &&
        securedTxAidValid &&
        securedTxAid == 37 &&
        duplicateEnv != nullptr &&
        std::string(duplicateEnv) == "1";

    // SPATEM-specific physical duplicate injection.
    // Disabled unless explicitly requested for ITS-AID 137.
    const char* spatemDuplicateEnv =
        std::getenv("V2X_SPATEM_DUPLICATE");

    const bool duplicateSpatem =
        ctx &&
        securedTxAidValid &&
        securedTxAid == 137 &&
        spatemDuplicateEnv != nullptr &&
        std::string(spatemDuplicateEnv) == "1";

    //----------------------------------------------------------------------
    // Deferred send
    //----------------------------------------------------------------------

    Simulator::ScheduleNow(
        [ctx, socket, ns3Packet, duplicateDenm, duplicateSpatem]()
        {

            //--------------------------------------------------------------
            // Internal Router transmissions without application context
            //--------------------------------------------------------------

            if (!ctx)
            {
                socket->SendTo(
                    ns3Packet,
                    0,
                    InetSocketAddress(
                        Ipv4Address(
                            "10.0.0.255"),
                        5000));

                return;
            }

            //------------------------------------------------------------------
            // TEST HOOK: send an identical DENM frame again.
            //
            // The first SEND follows the normal path below. The duplicate is
            // scheduled 100 us later so that it is an independent wire
            // transmission while preserving exactly the same bytes.
            //------------------------------------------------------------------

            if (duplicateDenm)
            {
                Ptr<Packet> duplicatePacket =
                    ns3Packet->Copy();

                Simulator::Schedule(
                    MicroSeconds(100),
                    [socket, duplicatePacket]()
                    {
                        const int duplicateSent =
                            socket->SendTo(
                                duplicatePacket,
                                0,
                                InetSocketAddress(
                                    Ipv4Address(
                                        "10.0.0.255"),
                                    5000));

                        NS_LOG_UNCOND(
                            "[DENM_V3_DUPLICATE] "
                            "aid=37 "
                            "delay_us=100 "
                            "sent_bytes="
                            << duplicateSent
                            << " packet_bytes="
                            << duplicatePacket->GetSize());
                    });
            }

            //------------------------------------------------------------------
            // TEST HOOK: exact duplicate of an already serialized SPATEM.
            // The copy preserves Packet tags, including PacketIdTag.
            // Normal operation is unchanged unless V2X_SPATEM_DUPLICATE=1.
            //------------------------------------------------------------------

            if (duplicateSpatem)
            {
                Ptr<Packet> duplicatePacket =
                    ns3Packet->Copy();

                const uint64_t duplicatePacketId =
                    ctx->packetId;

                Simulator::Schedule(
                    MicroSeconds(100),
                    [socket, duplicatePacket, duplicatePacketId]()
                    {
                        const int duplicateSent =
                            socket->SendTo(
                                duplicatePacket,
                                0,
                                InetSocketAddress(
                                    Ipv4Address("10.0.0.255"),
                                    5000));

                        NS_LOG_UNCOND(
                            "[SPATEM_PHYSICAL_DUPLICATE] "
                            "aid=137 packet_id="
                            << duplicatePacketId
                            << " delay_us=100 sent_bytes="
                            << duplicateSent
                            << " packet_bytes="
                            << duplicatePacket->GetSize());
                    });
            }

            //--------------------------------------------------------------
            // STEP 7 : SEND
            //--------------------------------------------------------------

            ctx->AdvancePipeline(
                PipelineStage::SEND);

            V2XWriteLog(
                *ctx,
                V2XLogLevel::DEBUG,
                "ACCESS",
                "SEND_BEGIN",
                "ns3_packet_bytes="
                + std::to_string(
                      ns3Packet->GetSize()));

            int sent =
                socket->SendTo(
                    ns3Packet,
                    0,
                    InetSocketAddress(
                        Ipv4Address(
                            "10.0.0.255"),
                        5000));

            if (sent < 0)
            {
                ctx->MarkFailure();

                ctx->reason =
                    "SendTo failed";

                V2XWriteLog(
                    *ctx,
                    V2XLogLevel::ERROR,
                    "ACCESS",
                    "SEND_FAIL",
                    "SendTo returned "
                    + std::to_string(
                          sent));

                TxBridgeContext::Pop();

                return;
            }

            ctx->MarkSuccess();

            ctx->reason =
                "SendTo OK";

            V2XWriteLog(
                *ctx,
                V2XLogLevel::INFO,
                "ACCESS",
                "SEND_OK",
                "bytes="
                + std::to_string(
                      sent));

            //--------------------------------------------------------------
            // STEP 8 : TX_COMPLETE
            //--------------------------------------------------------------

            ctx->AdvancePipeline(
                PipelineStage::TX_COMPLETE);

            ctx->MarkSuccess();

            V2XWriteLog(
                *ctx,
                V2XLogLevel::INFO,
                "TX",
                "TX_COMPLETE",
                "Transmission completed");

            TxBridgeContext::Pop();
        });
}

} // namespace ns3
