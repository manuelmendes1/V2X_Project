#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"
#include "vanetza_integration/cpm/rx/cpm_rx_pipeline.h"
#include "vanetza_integration/cpm/transport/cpm_transport_policy.h"
#include "vanetza_integration/cpm/types/cpm_input.h"

#include <vanetza/asn1/cpm.hpp>
#include <vanetza/asn1/its/r2/CpmContainerId.h>
#include <vanetza/asn1/its/r2/WrappedCpmContainer.h>
#include <vanetza/asn1/its/r2/PerceivedObjectContainer.h>

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

// Unit-level security boundary fixture. This injects a successful neutral
// verdict; it does not exercise Vanetza signature verification.
ns3::PacketContext MakeVerifiedContext()
{
    auto context = MakeContext();
    context.verificationPerformed = true;
    context.verificationSucceeded = true;
    context.verificationReport = 0;
    context.verifiedItsAid = ns3::kCpmItsAid;
    return context;
}


CpmInput MakeTwoObjectInput()
{
    CpmInput input = MakeValidInput();

    CpmPerceivedObject second = input.perceivedObjects.front();
    second.objectId = 502;
    second.position.xEast.value = 18.0;

    input.perceivedObjects.push_back(second);
    input.knownObjectCount = 7;
    return input;
}

std::vector<std::uint8_t> ReencodeWithDuplicateObjectId(
    const std::vector<std::uint8_t>& bytes)
{
    using AsnCpm = vanetza::asn1::r2::Cpm;

    AsnCpm message;
    if (!message.decode(bytes.data(), bytes.size()))
    {
        return {};
    }

    auto* root = message.operator->();
    if (!root)
    {
        return {};
    }

    auto& containers = root->payload.cpmContainers.list;

    for (int index = 0; index < containers.count; ++index)
    {
        auto* wrapped = containers.array[index];

        if (!wrapped ||
            wrapped->containerId !=
                Vanetza_ITS2_CpmContainerId_perceivedObjectContainer ||
            wrapped->containerData.present !=
                Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer)
        {
            continue;
        }

        auto& objects =
            wrapped->containerData.choice.PerceivedObjectContainer
                .perceivedObjects.list;

        if (objects.count < 2 ||
            !objects.array ||
            !objects.array[0] ||
            !objects.array[1] ||
            !objects.array[0]->objectId ||
            !objects.array[1]->objectId)
        {
            return {};
        }

        // Fixture de receção: preservar o PDU ASN.1, mas duplicar
        // semanticamente o identificador do segundo objeto.
        *objects.array[1]->objectId = *objects.array[0]->objectId;

        const auto reencoded = message.encode();
        return {reencoded.begin(), reencoded.end()};
    }

    return {};
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

    // Construir um PDU válido no nível ASN.1, mas com objectId duplicado.
    const auto twoObjectEncoded =
        CpmUperEncoder::Encode(MakeTwoObjectInput());

    Check(
        "TWO_DISTINCT_OBJECTS_FOR_DUPLICATE_FIXTURE_ENCODED",
        twoObjectEncoded.success && !twoObjectEncoded.bytes.empty());

    const auto duplicateObjectPayload =
        twoObjectEncoded.success
            ? ReencodeWithDuplicateObjectId(twoObjectEncoded.bytes)
            : std::vector<std::uint8_t>{};

    Check(
        "DUPLICATE_OBJECT_ID_FIXTURE_CREATED",
        !duplicateObjectPayload.empty() &&
            duplicateObjectPayload != twoObjectEncoded.bytes);

    if (!duplicateObjectPayload.empty())
    {
        PacketContext duplicateObjectContext = MakeVerifiedContext();
        CpmReplayContext duplicateObjectReplay{};
        CpmReceptionQueue duplicateObjectQueue{};
        CpmMessage duplicateObjectMessage{};

        const bool duplicateObjectResult = CpmRxPipeline::Process(
            duplicateObjectContext,
            duplicateObjectPayload,
            duplicateObjectMessage,
            987654322,
            duplicateObjectReplay,
            duplicateObjectQueue);

        Check(
            "DUPLICATE_OBJECT_ID_REJECTED_BEFORE_DELIVERY",
            !duplicateObjectResult &&
                duplicateObjectContext.pipelineStage == PipelineStage::DECODE &&
                duplicateObjectContext.pipelineResult == PipelineResult::FAIL &&
                duplicateObjectContext.reason ==
                    "cpm_object_id_missing_or_duplicate");

        Check(
            "DUPLICATE_OBJECT_ID_NOT_ENQUEUED",
            duplicateObjectQueue.Size() == 0 &&
                duplicateObjectContext.pipelineStage != PipelineStage::DELIVER);
    }

    // Etapa 10.10 RED: a mensagem CPM não pode ser entregue
    // quando não existe resultado de verificação criptográfica.
    PacketContext unverifiedContext = MakeContext();
    CpmReplayContext unverifiedReplay{};
    CpmReceptionQueue unverifiedQueue{};
    CpmMessage unverifiedMessage{};

    const bool unverifiedResult = CpmRxPipeline::Process(
        unverifiedContext,
        encoded.bytes,
        unverifiedMessage,
        987654320,
        unverifiedReplay,
        unverifiedQueue);

    Check("UNVERIFIED_CPM_REJECTED", !unverifiedResult);
    Check(
        "UNVERIFIED_CPM_STOPS_AT_VERIFY",
        !unverifiedResult &&
            unverifiedContext.pipelineStage == PipelineStage::VERIFY &&
            unverifiedContext.pipelineResult == PipelineResult::DROP);
    Check(
        "UNVERIFIED_CPM_NOT_ENQUEUED",
        unverifiedQueue.Size() == 0);

    PacketContext failedVerificationContext = MakeContext();
    failedVerificationContext.verificationPerformed = true;
    failedVerificationContext.verificationSucceeded = false;
    failedVerificationContext.verificationReport = 1;
    failedVerificationContext.verifiedItsAid = kCpmItsAid;

    CpmReplayContext failedVerificationReplay{};
    CpmReceptionQueue failedVerificationQueue{};
    CpmMessage failedVerificationMessage{};

    const bool failedVerificationResult = CpmRxPipeline::Process(
        failedVerificationContext,
        encoded.bytes,
        failedVerificationMessage,
        987654320,
        failedVerificationReplay,
        failedVerificationQueue);

    Check(
        "FAILED_VERIFICATION_CPM_REJECTED",
        !failedVerificationResult);
    Check(
        "FAILED_VERIFICATION_STOPS_AT_VERIFY",
        !failedVerificationResult &&
            failedVerificationContext.pipelineStage == PipelineStage::VERIFY &&
            failedVerificationContext.pipelineResult == PipelineResult::DROP &&
            failedVerificationContext.reason ==
                "cpm_cryptographic_verification_failed");
    Check(
        "FAILED_VERIFICATION_NOT_ENQUEUED",
        failedVerificationQueue.Size() == 0);

    PacketContext wrongVerifiedAidContext = MakeContext();
    wrongVerifiedAidContext.verificationPerformed = true;
    wrongVerifiedAidContext.verificationSucceeded = true;
    wrongVerifiedAidContext.verifiedItsAid = 37;

    CpmReplayContext wrongVerifiedAidReplay{};
    CpmReceptionQueue wrongVerifiedAidQueue{};
    CpmMessage wrongVerifiedAidMessage{};

    const bool wrongVerifiedAidResult = CpmRxPipeline::Process(
        wrongVerifiedAidContext,
        encoded.bytes,
        wrongVerifiedAidMessage,
        987654320,
        wrongVerifiedAidReplay,
        wrongVerifiedAidQueue);

    Check(
        "WRONG_VERIFIED_AID_CPM_REJECTED",
        !wrongVerifiedAidResult);
    Check(
        "WRONG_VERIFIED_AID_STOPS_AT_VERIFY",
        !wrongVerifiedAidResult &&
            wrongVerifiedAidContext.pipelineStage == PipelineStage::VERIFY &&
            wrongVerifiedAidContext.pipelineResult == PipelineResult::DROP &&
            wrongVerifiedAidContext.reason ==
                "cpm_verified_its_aid_mismatch");
    Check(
        "WRONG_VERIFIED_AID_NOT_ENQUEUED",
        wrongVerifiedAidQueue.Size() == 0);

    CpmReplayContext replay{};
    CpmReceptionQueue receptionQueue{};
    PacketContext context = MakeVerifiedContext();
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

    PacketContext duplicateContext = MakeVerifiedContext();
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

    PacketContext malformedContext = MakeVerifiedContext();
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

    PacketContext badAidContext = MakeVerifiedContext();
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

    PacketContext identityMismatchContext = MakeVerifiedContext();
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

    PacketContext fullQueueContext = MakeVerifiedContext();
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
