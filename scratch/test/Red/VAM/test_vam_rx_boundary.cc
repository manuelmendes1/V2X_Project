#include "vanetza_integration/vam/transport/vam_rx_boundary.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
int failures = 0;

void Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    if (!condition) {
        ++failures;
    }
}

void ConfigureValidVamContext(ns3::PacketContext& context)
{
    context.packetId = 17;
    context.senderStationId = 42;
    context.receiverStationId = 84;
    context.btpDestinationPort = 2018;

    context.geonet.upperProtocol = "BTP_B";
    context.geonet.transportType = "SHB";
    context.geonet.hasItsAid = true;
    context.geonet.itsAid = 638;
    context.geonet.hasTrafficClass = true;
    context.geonet.trafficClass = 5;
    context.geonet.hasLifetime = true;
    context.geonet.lifetimeMs = 1000.0;
    context.geonet.hasSenderMid = true;
    context.geonet.senderMid = {1, 2, 3, 4, 5, 6};
}

void CheckRejectedWithoutPartialPayload(
    ns3::PacketContext& context,
    const std::vector<uint8_t>& bytes,
    const char* label)
{
    const bool accepted = ns3::VamRxBoundary::Process(context, bytes);
    Check(!accepted, label);
    Check(context.pipelineStage == ns3::PipelineStage::BTP_RX,
          "REJECTED_VAM_STOPS_AT_BTP_RX");
    Check(context.pipelineResult == ns3::PipelineResult::FAIL,
          "REJECTED_VAM_MARKS_BTP_RX_FAILURE");
    Check(context.payload.bytes.empty(),
          "REJECTED_VAM_HAS_NO_PARTIAL_PAYLOAD");
}
} // namespace

int main()
{
    const std::vector<uint8_t> encoded{0x21, 0x43, 0x65, 0x87};

    ns3::PacketContext accepted;
    ConfigureValidVamContext(accepted);

    const bool acceptedResult =
        ns3::VamRxBoundary::Process(accepted, encoded);

    Check(acceptedResult, "VALID_VAM_INDICATION_ACCEPTED");
    Check(accepted.pipelineStage == ns3::PipelineStage::BTP_RX,
          "VAM_BOUNDARY_STOPS_AT_BTP_RX");
    Check(accepted.pipelineStep == 4 && accepted.pipelineTotal == 9,
          "VAM_BTP_RX_PIPELINE_POSITION_4_OF_9");
    Check(accepted.pipelineResult == ns3::PipelineResult::OK,
          "VAM_BTP_RX_MARKED_SUCCESS");
    Check(accepted.payload.type == ns3::PayloadType::VAM,
          "VAM_PAYLOAD_TYPE_RECORDED");
    Check(accepted.payload.bytes == encoded,
          "VAM_UPER_BYTES_PRESERVED");
    Check(accepted.geonet.itsAid == 638 && accepted.geonet.hasItsAid,
          "VAM_ITS_AID_METADATA_PRESERVED");
    Check(accepted.geonet.trafficClass == 5 &&
              accepted.geonet.hasTrafficClass,
          "VAM_TRAFFIC_CLASS_METADATA_PRESERVED");
    Check(accepted.geonet.lifetimeMs == 1000.0 &&
              accepted.geonet.hasLifetime,
          "VAM_LIFETIME_METADATA_PRESERVED");
    Check(accepted.geonet.senderMid ==
              std::array<uint8_t, 6>{1, 2, 3, 4, 5, 6} &&
              accepted.geonet.hasSenderMid,
          "VAM_SENDER_MID_METADATA_PRESERVED");
    Check(accepted.packetId == 17 && accepted.senderStationId == 42 &&
              accepted.receiverStationId == 84,
          "VAM_PACKET_AND_ENDPOINT_IDENTITY_PRESERVED");

    ns3::PacketContext wrongPort;
    ConfigureValidVamContext(wrongPort);
    wrongPort.btpDestinationPort = 2001;
    CheckRejectedWithoutPartialPayload(
        wrongPort, encoded, "WRONG_BTP_PORT_REJECTED");

    ns3::PacketContext missingAid;
    ConfigureValidVamContext(missingAid);
    missingAid.geonet.hasItsAid = false;
    CheckRejectedWithoutPartialPayload(
        missingAid, encoded, "MISSING_ITS_AID_REJECTED");

    ns3::PacketContext wrongAid;
    ConfigureValidVamContext(wrongAid);
    wrongAid.geonet.itsAid = 639;
    CheckRejectedWithoutPartialPayload(
        wrongAid, encoded, "WRONG_ITS_AID_REJECTED");

    ns3::PacketContext emptyPayload;
    ConfigureValidVamContext(emptyPayload);
    CheckRejectedWithoutPartialPayload(
        emptyPayload, {}, "EMPTY_VAM_PAYLOAD_REJECTED");

    if (failures != 0) {
        std::cerr << "VAM_RX_BOUNDARY_FAILURES=" << failures << '\n';
        return 1;
    }

    std::cout << "VAM_RX_BOUNDARY_TESTS=PASS\n";
    return 0;
}
