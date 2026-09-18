#include "denm_sender.h"

#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/denm/action/denm_action_id_manager.h"
#include "vanetza_integration/context/tx_bridge/tx_bridge_context.h"
#include "vanetza_integration/core/v2x_log.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"

#include "ns3/assert.h"
#include "ns3/simulator.h"

#include <cmath>
#include <string>

namespace ns3
{

bool
DenmSender::SendRaw(
    const std::shared_ptr<PacketContext>& context,
    VehicleState& vehicle,
    const DenmInput& input,
    const std::vector<uint8_t>& rawPayload,
    double disseminationRadiusMeters,
    const std::string& reason)
{
    NS_ASSERT(context != nullptr);

    if (rawPayload.empty())
    {
        return false;
    }

    if (input.actionId.sequenceNumber == 0)
    {
        return false;
    }

    if (!std::isfinite(disseminationRadiusMeters) ||
        disseminationRadiusMeters <= 0.0)
    {
        return false;
    }

    PacketContext& ctx =
        *context;

    ctx.simulationStep =
        SimulationContext::simulationStep;

    ctx.simulationTime =
        Simulator::Now().GetSeconds();

    //----------------------------------------------------------------------
    // Same TX preparation contract as CAM.
    //----------------------------------------------------------------------

    if (!vehicle.runtime ||
        !vehicle.runtime->PrepareTx())
    {
        ctx.MarkFailure();
        ctx.reason =
            "tx_prepare_failed";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "TX_PREPARE_FAIL",
            "runtime TX preparation failed");

        return false;
    }

    //----------------------------------------------------------------------
    // Keep PacketContext alive until TX_COMPLETE.
    //----------------------------------------------------------------------

    TxBridgeContext::Push(
        context);

    //----------------------------------------------------------------------
    // Step 4/8 : BTP_TX
    //----------------------------------------------------------------------

    ctx.AdvancePipeline(
        PipelineStage::BTP_TX);

    BtpTxRequest request{};

    request.destinationPort =
        2002;

    request.destinationPortInfo =
        0;

    request.transport =
        BtpTransportType::GBC;

    request.communicationProfile =
        BtpCommunicationProfile::ITS_G5;

    request.itsAid =
        37; // aid::DEN

    request.trafficClass =
        0;

    BtpGeoArea area{};

    area.latitudeDeg =
        input.eventPosition.latitude;

    area.longitudeDeg =
        input.eventPosition.longitude;

    area.radiusMeters =
        disseminationRadiusMeters;

    request.destinationArea =
        area;

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "DENM",
        "GBC_REQUEST",
        "port=2002"
        " aid=37"
        " transport=GBC"
        " radius_m=" +
        std::to_string(
            disseminationRadiusMeters) +
        " action_seq=" +
        std::to_string(
            input.actionId.sequenceNumber));

    const bool sent =
        vehicle.runtime->SendBtpB(
            ctx,
            request,
            rawPayload);

    if (!sent)
    {
        ctx.MarkFailure();
        ctx.reason =
            "btp_send_failed";

        V2XWriteLog(
            ctx,
            V2XLogLevel::ERROR,
            "DENM",
            "BTP_TX_FAIL",
            "DENM BTP/GBC transmission request failed");

        //------------------------------------------------------------------
        // Roll back the locally-open ActionID so a failed baseline
        // transmission does not leave a permanently active action.
        //------------------------------------------------------------------

        DenmActionIdManager::Instance().Apply(
            input.actionId,
            DenmManagementAction::CANCELLATION);

        TxBridgeContext::Pop();

        return false;
    }

    V2XWriteLog(
        ctx,
        V2XLogLevel::DEBUG,
        "DENM",
        "BTP_TX_OK",
        "port=2002"
        " aid=37"
        " action_seq=" +
        std::to_string(
            input.actionId.sequenceNumber) +
        " reason=" +
        reason);

    //----------------------------------------------------------------------
    // ACCESS_TX / SEND / TX_COMPLETE continue asynchronously downstream.
    //----------------------------------------------------------------------

    return true;
}

} // namespace ns3
