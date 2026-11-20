#include "vanetza_integration/vam/transport/vam_rx_boundary.h"

#include "vanetza_integration/vam/transport/vam_transport_policy.h"

namespace ns3
{
namespace
{

bool Reject(PacketContext& context, const char* reason)
{
    context.reason = reason;
    context.MarkFailure();
    return false;
}

} // namespace

bool
VamRxBoundary::Process(
    PacketContext& context,
    const std::vector<uint8_t>& payload)
{
    // The runtime has already completed RAW_RX and ACCESS_RX using
    // this PacketContext. Complete the common upper-layer RX stages.
    context.AdvancePipeline(PipelineStage::GEONET_RX);
    context.MarkSuccess();

    context.AdvancePipeline(PipelineStage::BTP_RX);

    if (context.btpDestinationPort != kVamBtpPort)
    {
        return Reject(context, "invalid_vam_btp_port");
    }

    if (!context.geonet.hasItsAid ||
        context.geonet.itsAid != kVamItsAid)
    {
        return Reject(context, "invalid_vam_its_aid");
    }

    if (payload.empty())
    {
        return Reject(context, "empty_vam_payload");
    }

    // Commit payload state only after every boundary check succeeds.
    context.payload.type = PayloadType::VAM;
    context.payload.bytes = payload;

    context.MarkSuccess();
    return true;
}

} // namespace ns3
