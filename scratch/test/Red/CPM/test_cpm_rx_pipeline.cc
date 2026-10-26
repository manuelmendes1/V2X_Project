#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"
#include "vanetza_integration/cpm/rx/cpm_rx_pipeline.h"
#include "vanetza_integration/cpm/transport/cpm_transport_policy.h"
#include "vanetza_integration/cpm/types/cpm_input.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace
{

unsigned passed = 0;
unsigned failed = 0;

bool Check(const char* name, bool condition)
{
    std::cout << (condition ? "PASS " : "FAIL ")
              << name << '\n';

    if (condition) {
        ++passed;
    } else {
        ++failed;
    }

    return condition;
}

CpmInput MakeValidInput()
{
    CpmInput input{};
    input.station.stationId = 9001;
    input.station.origin = CpmRoadSideUnitStationData{};
    input.referenceTimeItsMs = 123456;
    input.referencePosition.latitudeDeg = 41.3852;
    input.referencePosition.longitudeDeg = -8.4328;
    input.knownObjectCount = 7;

    CpmPerceivedObject object{};
    object.objectId = 501;
    object.measurementDeltaTimeMs = -12;
    object.position.xEast.value = 12.0;
    object.position.yNorth.value = -3.5;
    object.objectAgeMs = 400;

    input.perceivedObjects.push_back(object);
    return input;
}

ns3::PacketContext MakeContext()
{
    ns3::PacketContext context{};
    context.senderStationId = 9001;
    context.receiverStationId = 9002;
    context.senderNodeId = 7;
    context.receiverNodeId = 8;
    context.btpDestinationPort = ns3::kCpmBtpPort;
    context.geonet.hasItsAid = true;
    context.geonet.itsAid = ns3::kCpmItsAid;
    return context;
}

} // namespace

int main()
{
    using namespace ns3;

    const auto encoded = CpmUperEncoder::Encode(MakeValidInput());

    if (!Check(
            "VALID_CPM_FIXTURE_ENCODED",
            encoded.success && !encoded.bytes.empty())) {
        std::cout << "CPM_RX_PIPELINE=FAIL\n";
        return 1;
    }

    CpmReplayContext replay{};
    CpmReceptionQueue receptionQueue{};
    PacketContext context = MakeContext();
    CpmMessage delivered{};

    const bool result = CpmRxPipeline::Process(
        context,
        encoded.bytes,
        delivered,
        987654321,
        replay,
        receptionQueue);

    Check("VALID_CPM_DELIVERED", result);
    Check(
        "DELIVER_STAGE_REACHED",
        result && context.pipelineStage == PipelineStage::DELIVER);
    Check(
        "DELIVER_RESULT_OK",
        result && context.pipelineResult == PipelineResult::OK);
    Check(
        "NEUTRAL_STATION_ID_PRESERVED",
        result && delivered.station.stationId == 9001);
    Check(
        "NEUTRAL_OBJECT_COUNT_PRESERVED",
        result && delivered.knownObjectCount == 7 &&
            delivered.perceivedObjects.size() == 1);
    Check(
        "NEUTRAL_OBJECT_SEMANTICS_PRESERVED",
        result && delivered.perceivedObjects.front().objectId == 501 &&
            delivered.perceivedObjects.front().measurementDeltaTimeMs == -12);
    Check(
        "LOCAL_RECEPTION_METADATA_PRESERVED",
        result && delivered.receivedAtUnixMs == 987654321);
    Check(
        "PAYLOAD_TYPE_SET_TO_CPM",
        context.payload.type == PayloadType::CPM);
    Check(
        "APPLICATION_QUEUE_CONTAINS_DELIVERED_CPM",
        receptionQueue.Size() == 1);

    PacketContext duplicateContext = MakeContext();
    CpmMessage duplicateMessage{};

    const bool duplicateResult = CpmRxPipeline::Process(
        duplicateContext,
        encoded.bytes,
        duplicateMessage,
        987654322,
        replay,
        receptionQueue);

    Check("EXACT_DUPLICATE_REJECTED", !duplicateResult);
    Check(
        "DUPLICATE_DOES_NOT_DELIVER",
        duplicateContext.pipelineStage == PipelineStage::REPLAY &&
            duplicateContext.pipelineResult == PipelineResult::DROP);
    Check(
        "DUPLICATE_REASON_PRESERVED",
        duplicateContext.reason == "cpm_exact_payload_replay_detected");
    Check(
        "DUPLICATE_NOT_ENQUEUED",
        receptionQueue.Size() == 1);

    CpmMessage applicationMessage{};
    Check(
        "APPLICATION_CONSUMER_CAN_POP",
        receptionQueue.TryPop(applicationMessage));
    Check(
        "APPLICATION_MESSAGE_STATION_ID_PRESERVED",
        applicationMessage.station.stationId == 9001);
    Check(
        "APPLICATION_MESSAGE_OBJECTS_PRESERVED",
        applicationMessage.perceivedObjects.size() == 1 &&
            applicationMessage.perceivedObjects.front().objectId == 501);
    Check(
        "APPLICATION_QUEUE_EMPTY_AFTER_POP",
        receptionQueue.Empty());

    PacketContext malformedContext = MakeContext();
    CpmReplayContext malformedReplay{};
    CpmReceptionQueue malformedQueue{};
    CpmMessage malformedMessage{};

    const bool malformedResult = CpmRxPipeline::Process(
        malformedContext,
        std::vector<std::uint8_t>{0xDE, 0xAD},
        malformedMessage,
        987654323,
        malformedReplay,
        malformedQueue);

    Check("MALFORMED_CPM_REJECTED", !malformedResult);
    Check(
        "MALFORMED_CPM_DOES_NOT_DELIVER",
        malformedContext.pipelineStage == PipelineStage::DECODE &&
            malformedContext.pipelineResult == PipelineResult::FAIL);

    PacketContext badAidContext = MakeContext();
    badAidContext.geonet.itsAid = 37;
    CpmReplayContext badAidReplay{};
    CpmReceptionQueue badAidQueue{};
    CpmMessage badAidMessage{};

    const bool badAidResult = CpmRxPipeline::Process(
        badAidContext,
        encoded.bytes,
        badAidMessage,
        987654324,
        badAidReplay,
        badAidQueue);

    Check("INVALID_CPM_AID_REJECTED", !badAidResult);
    Check(
        "INVALID_AID_DOES_NOT_DELIVER",
        badAidContext.pipelineStage == PipelineStage::VERIFY &&
            badAidContext.pipelineResult == PipelineResult::FAIL);

    PacketContext identityMismatchContext = MakeContext();
    identityMismatchContext.senderStationId = 9003;
    CpmReplayContext identityMismatchReplay{};
    CpmReceptionQueue identityMismatchQueue{};
    CpmMessage identityMismatchMessage{};

    const bool identityMismatchResult = CpmRxPipeline::Process(
        identityMismatchContext,
        encoded.bytes,
        identityMismatchMessage,
        987654325,
        identityMismatchReplay,
        identityMismatchQueue);

    Check("SENDER_STATION_MISMATCH_REJECTED", !identityMismatchResult);
    Check(
        "IDENTITY_MISMATCH_DOES_NOT_DELIVER",
        identityMismatchContext.pipelineStage == PipelineStage::DECODE &&
            identityMismatchContext.pipelineResult == PipelineResult::DROP);

    PacketContext fullQueueContext = MakeContext();
    CpmReplayContext fullQueueReplay{};
    CpmReceptionQueue fullQueue{};
    CpmMessage placeholder{};

    for (std::size_t i = 0; i < CpmReceptionQueue::kCapacity; ++i)
    {
        if (!fullQueue.TryPush(placeholder))
        {
            std::cerr << "FAIL TEST_QUEUE_SETUP\\n";
            return 2;
        }
    }

    CpmMessage fullQueueMessage{};
    const bool fullQueueResult = CpmRxPipeline::Process(
        fullQueueContext,
        encoded.bytes,
        fullQueueMessage,
        987654326,
        fullQueueReplay,
        fullQueue);

    Check("FULL_APPLICATION_QUEUE_REJECTS_CPM", !fullQueueResult);
    Check(
        "FULL_QUEUE_DOES_NOT_REACH_DELIVER",
        fullQueueContext.pipelineStage == PipelineStage::DECODE &&
            fullQueueContext.pipelineResult == PipelineResult::DROP);
    Check(
        "FULL_QUEUE_REASON_PRESERVED",
        fullQueueContext.reason == "cpm_application_reception_queue_full");

    std::cout << "CPM_RX_PIPELINE_PASSED=" << passed << '\n';
    std::cout << "CPM_RX_PIPELINE_FAILED=" << failed << '\n';
    std::cout << "CPM_RX_PIPELINE="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
