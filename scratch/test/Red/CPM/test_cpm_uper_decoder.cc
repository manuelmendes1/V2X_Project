#include "vanetza_integration/cpm/codec/cpm_uper_decoder.h"
#include "vanetza_integration/cpm/codec/cpm_uper_encoder.h"
#include "vanetza_integration/cpm/types/cpm_input.h"

#include <vanetza/asn1/cpm.hpp>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/WrappedCpmContainer.h>
#include <vanetza/asn1/its/r2/PerceivedObjectContainer.h>
#include <vanetza/asn1/its/r2/PerceivedObject.h>
#include <vanetza/asn1/its/r2/ObjectPerceptionQuality.h>
#include <vanetza/asn1/its/r2/Longitude.h>
#include <vanetza/asn1/its/r2/CartesianCoordinateLarge.h>
#include <vanetza/asn1/its/r2/CpmContainerId.h>
#include <vanetza/asn1/support/asn_SEQUENCE_OF.h>
extern "C"
{
#include <vanetza/asn1/support/asn_application.h>
}

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace
{

using AsnCpm = vanetza::asn1::r2::Cpm;
using AsnRoot = Vanetza_ITS2_CollectivePerceptionMessage_t;

unsigned passed = 0;
unsigned failed = 0;

bool Check(const std::string& name, bool condition)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
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

std::vector<std::uint8_t> ReencodeWithHeaderMutation(
    const std::vector<std::uint8_t>& bytes,
    const std::function<void(AsnRoot&)>& mutate)
{
    AsnCpm message;

    if (!message.decode(bytes.data(), bytes.size())) {
        return {};
    }

    auto* root = message.operator->();
    if (!root) {
        return {};
    }

    mutate(*root);

    const auto reencoded = message.encode();
    return {reencoded.begin(), reencoded.end()};
}

std::vector<std::uint8_t> ReencodeFirstObjectMutation(
    const std::vector<std::uint8_t>& bytes,
    const std::function<void(Vanetza_ITS2_PerceivedObject_t&)>& mutate)
{
    AsnCpm message;

    if (!message.decode(bytes.data(), bytes.size())) {
        return {};
    }

    auto* root = message.operator->();
    if (!root) {
        return {};
    }

    auto& containers = root->payload.cpmContainers.list;

    for (int i = 0; i < containers.count; ++i) {
        auto* wrapped = containers.array[i];

        if (!wrapped ||
            wrapped->containerId !=
                Vanetza_ITS2_CpmContainerId_perceivedObjectContainer ||
            wrapped->containerData.present !=
                Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer) {
            continue;
        }

        auto& objects =
            wrapped->containerData.choice.PerceivedObjectContainer
                .perceivedObjects.list;

        if (objects.count < 1 || !objects.array || !objects.array[0]) {
            return {};
        }

        mutate(*objects.array[0]);

        const auto reencoded = message.encode();
        return {reencoded.begin(), reencoded.end()};
    }

    return {};
}

bool RemoveContainerById(AsnRoot& root, long containerId)
{
    auto& containers = root.payload.cpmContainers.list;

    if (containers.count < 0 ||
        (containers.count > 0 && containers.array == nullptr))
    {
        return false;
    }

    for (int index = 0; index < containers.count; ++index)
    {
        auto* container = containers.array[index];
        if (!container ||
            static_cast<long>(container->containerId) != containerId)
        {
            continue;
        }

        ASN_STRUCT_FREE(
            asn_DEF_Vanetza_ITS2_WrappedCpmContainer,
            container);

        for (int next = index + 1; next < containers.count; ++next)
        {
            containers.array[next - 1] = containers.array[next];
        }

        --containers.count;
        if (containers.array && containers.count < containers.size)
        {
            containers.array[containers.count] = nullptr;
        }
        return true;
    }

    return false;
}

std::vector<std::uint8_t> ReencodeWithoutContainer(
    const std::vector<std::uint8_t>& bytes,
    long containerId)
{
    AsnCpm message;
    if (!message.decode(bytes.data(), bytes.size()))
    {
        return {};
    }

    auto* root = message.operator->();
    if (!root || !RemoveContainerById(*root, containerId))
    {
        return {};
    }

    const auto reencoded = message.encode();
    return {reencoded.begin(), reencoded.end()};
}

std::vector<std::uint8_t> ReencodeEmptyPerceivedObjectList(
    const std::vector<std::uint8_t>& bytes)
{
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
    if (!containers.array)
    {
        return {};
    }

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

        if (objects.count < 0 ||
            (objects.count > 0 && objects.array == nullptr))
        {
            return {};
        }

        for (int objectIndex = 0; objectIndex < objects.count; ++objectIndex)
        {
            if (objects.array[objectIndex])
            {
                ASN_STRUCT_FREE(
                    asn_DEF_Vanetza_ITS2_PerceivedObject,
                    objects.array[objectIndex]);
                objects.array[objectIndex] = nullptr;
            }
        }
        objects.count = 0;

        const auto reencoded = message.encode();
        return {reencoded.begin(), reencoded.end()};
    }

    return {};
}

bool CheckDecodeFailure(
    const std::string& name,
    const std::vector<std::uint8_t>& payload)
{
    const auto result = CpmUperDecoder::Decode(payload, 987654321);

    return Check(
        name,
        !result.success &&
            !result.message.has_value() &&
            !result.error.empty());
}

bool CheckValidRoundTrip(const CpmInput& input)
{
    const auto encoded = CpmUperEncoder::Encode(input);

    if (!Check(
            "VALID_RSU_INPUT_ENCODED",
            encoded.success && !encoded.bytes.empty())) {
        return false;
    }

    constexpr std::int64_t receptionTimeUnixMs = 987654321;
    const auto decoded =
        CpmUperDecoder::Decode(encoded.bytes, receptionTimeUnixMs);

    if (!Check(
            "DECODER_RETURNS_COMPLETE_MESSAGE",
            decoded.success &&
                decoded.message.has_value() &&
                decoded.error.empty())) {
        return false;
    }

    const CpmMessage& message = *decoded.message;

    Check("RSU_STATION_ID_PRESERVED",
          message.station.stationId == 9001);

    Check("RSU_ORIGIN_PRESERVED",
          std::holds_alternative<CpmRoadSideUnitStationData>(
              message.station.origin));

    Check("REFERENCE_TIME_PRESERVED",
          message.referenceTimeItsMs == input.referenceTimeItsMs);

    const bool referencePositionPreserved =
        message.referencePosition.latitudeDeg.has_value() &&
        message.referencePosition.longitudeDeg.has_value() &&
        std::abs(
            *message.referencePosition.latitudeDeg -
            *input.referencePosition.latitudeDeg) <= 1.0e-7 &&
        std::abs(
            *message.referencePosition.longitudeDeg -
            *input.referencePosition.longitudeDeg) <= 1.0e-7;

    Check("WGS84_REFERENCE_POSITION_PRESERVED",
          referencePositionPreserved);

    Check("UNAVAILABLE_REFERENCE_ALTITUDE_PRESERVED",
          !message.referencePosition.altitudeMeters.has_value() &&
          message.referencePosition.altitudeConfidenceMeters.state ==
              CpmAltitudeAccuracyState::Unavailable);

    Check("UNAVAILABLE_REFERENCE_CONFIDENCE_PRESERVED",
          message.referencePosition.confidenceEllipse
                  .semiMajorConfidenceMeters.state ==
              CpmSemiAxisState::Unavailable &&
          message.referencePosition.confidenceEllipse
                  .semiMinorConfidenceMeters.state ==
              CpmSemiAxisState::Unavailable &&
          message.referencePosition.confidenceEllipse
                  .semiMajorOrientationDeg.state ==
              CpmHeadingState::Unavailable);

    Check("KNOWN_OBJECT_COUNT_PRESERVED_FROM_WIRE",
          message.knownObjectCount == input.knownObjectCount &&
          message.knownObjectCount == 7);

    Check("PERCEIVED_OBJECT_LIST_PRESERVED",
          message.perceivedObjects.size() == 1);

    if (message.perceivedObjects.size() == 1) {
        const auto& object = message.perceivedObjects.front();

        Check("OBJECT_ID_AND_MEASUREMENT_TIME_PRESERVED",
              object.objectId == 501 &&
              object.measurementDeltaTimeMs == -12);

        Check("OBJECT_POSITION_PRESERVED",
              std::abs(object.position.xEast.value - 12.0) <= 0.01 &&
              std::abs(object.position.yNorth.value - (-3.5)) <= 0.01);

        Check("UNAVAILABLE_OBJECT_COORDINATE_CONFIDENCE_PRESERVED",
              !object.position.xEast.confidence.has_value() &&
              !object.position.yNorth.confidence.has_value());

        Check("OPTIONAL_OBJECT_AGE_PRESERVED",
              object.objectAgeMs.has_value() &&
              *object.objectAgeMs == 400);
    }

    Check("RECEPTION_TIMESTAMP_PRESERVED",
          message.receivedAtUnixMs == receptionTimeUnixMs);

    return true;
}


bool CheckGoldenFixture()
{
    // Fixture R2 fixa, construída diretamente com tipos ASN.1 gerados.
    // Não é gerada pelo CpmUperEncoder durante o teste.
    static const std::vector<std::uint8_t> goldenBytes{
        0x02, 0x0e, 0x00, 0x00, 0x23, 0x29, 0x00, 0x00,
        0x00, 0x07, 0x89, 0x02, 0x72, 0x7e, 0x53, 0x03,
        0x32, 0x18, 0x9e, 0x07, 0xff, 0xff, 0xff, 0x08,
        0xed, 0xdd, 0x0f, 0x91, 0x01, 0x00, 0x41, 0x10,
        0x10, 0x05, 0x00, 0x80, 0x0f, 0xab, 0xfa, 0x20,
        0x4b, 0x0f, 0xff, 0x7f, 0xa8, 0xbf, 0xfc, 0xc8,
        0x00
    };

    Check("GOLDEN_FIXTURE_BYTE_COUNT", goldenBytes.size() == 49);

    constexpr std::int64_t receptionTimeUnixMs = 1234567890;
    const auto result =
        CpmUperDecoder::Decode(goldenBytes, receptionTimeUnixMs);

    if (!Check(
            "GOLDEN_FIXTURE_DECODES",
            result.success &&
                result.message.has_value() &&
                result.error.empty()))
    {
        return false;
    }

    const CpmMessage& message = *result.message;

    Check("GOLDEN_STATION_HEADER_PRESERVED",
          message.station.stationId == 9001 &&
          std::holds_alternative<CpmRoadSideUnitStationData>(
              message.station.origin));

    Check("GOLDEN_REFERENCE_TIME_PRESERVED",
          message.referenceTimeItsMs == 123456);

    const bool positionPreserved =
        message.referencePosition.latitudeDeg.has_value() &&
        message.referencePosition.longitudeDeg.has_value() &&
        std::abs(*message.referencePosition.latitudeDeg - 41.3852) <= 1.0e-7 &&
        std::abs(*message.referencePosition.longitudeDeg - (-8.4328)) <= 1.0e-7;

    Check("GOLDEN_WGS84_POSITION_PRESERVED", positionPreserved);

    Check("GOLDEN_UNAVAILABLE_ALTITUDE_AND_CONFIDENCE_PRESERVED",
          !message.referencePosition.altitudeMeters.has_value() &&
          message.referencePosition.altitudeConfidenceMeters.state ==
              CpmAltitudeAccuracyState::Unavailable &&
          message.referencePosition.confidenceEllipse
                  .semiMajorConfidenceMeters.state ==
              CpmSemiAxisState::Unavailable &&
          message.referencePosition.confidenceEllipse
                  .semiMinorConfidenceMeters.state ==
              CpmSemiAxisState::Unavailable &&
          message.referencePosition.confidenceEllipse
                  .semiMajorOrientationDeg.state ==
              CpmHeadingState::Unavailable);

    Check("GOLDEN_KNOWN_COUNT_DISTINCT_FROM_INCLUDED_LIST",
          message.knownObjectCount == 2 &&
          message.perceivedObjects.size() == 1);

    bool objectPreserved = false;
    if (message.perceivedObjects.size() == 1)
    {
        const auto& object = message.perceivedObjects.front();

        objectPreserved =
            object.objectId == 501 &&
            object.measurementDeltaTimeMs == -12 &&
            std::abs(object.position.xEast.value - 12.0) <= 0.01 &&
            std::abs(object.position.yNorth.value - (-3.5)) <= 0.01 &&
            !object.position.xEast.confidence.has_value() &&
            !object.position.yNorth.confidence.has_value() &&
            object.objectAgeMs.has_value() &&
            *object.objectAgeMs == 400;
    }

    Check("GOLDEN_OBJECT_SEMANTICS_PRESERVED", objectPreserved);

    Check("GOLDEN_RECEPTION_TIME_IS_LOCAL_METADATA",
          message.receivedAtUnixMs == receptionTimeUnixMs);

    std::cout << "CPM_GOLDEN_FIXTURE_CONTRACT="
              << (failed == 0 ? "PASS" : "CHECK_GLOBAL_FAILURE_COUNT")
              << '\n';

    return true;
}

} // namespace

int main()
{
    CheckGoldenFixture();

    const CpmInput input = MakeValidInput();
    const auto encoded = CpmUperEncoder::Encode(input);

    if (!Check(
            "VALID_FIXTURE_ENCODED",
            encoded.success && !encoded.bytes.empty())) {
        std::cout << "CPM_UPER_DECODER_CONTRACT=FAIL\n";
        return 1;
    }

    CheckValidRoundTrip(input);

    const auto emptyObjectList =
        ReencodeEmptyPerceivedObjectList(encoded.bytes);
    Check("EMPTY_OBJECT_LIST_FIXTURE_CREATED",
          !emptyObjectList.empty() &&
          emptyObjectList != encoded.bytes);
    if (!emptyObjectList.empty() && emptyObjectList != encoded.bytes)
    {
        const auto emptyDecoded =
            CpmUperDecoder::Decode(emptyObjectList, 987654321);
        Check("EMPTY_OBJECT_LIST_ACCEPTED",
              emptyDecoded.success &&
              emptyDecoded.message.has_value() &&
              emptyDecoded.message->perceivedObjects.empty() &&
              emptyDecoded.message->knownObjectCount == 7);
    }

    CheckDecodeFailure("EMPTY_PAYLOAD_REJECTED", {});

    CheckDecodeFailure("SHORT_MALFORMED_PAYLOAD_REJECTED", {0x02});

    if (encoded.bytes.size() > 1) {
        auto truncated = encoded.bytes;
        truncated.resize(truncated.size() - 1);
        CheckDecodeFailure("TRUNCATED_PAYLOAD_REJECTED", truncated);
    } else {
        Check("ENCODED_FIXTURE_LONG_ENOUGH_TO_TRUNCATE", false);
    }

    // Um datagrama não deve ser aceite como CPM válido se contiver
    // bytes adicionais depois do fim do PDU UPER.
    auto trailingOctet = encoded.bytes;
    trailingOctet.push_back(0x00);
    CheckDecodeFailure(
        "TRAILING_OCTET_REJECTED",
        trailingOctet);

    // UPER preenche o último octeto com zeros quando o PDU não termina
    // numa fronteira de octeto. Alterar o bit menos significativo deve
    // ser rejeitado se esse bit pertencer ao padding.
    const unsigned usedBitsInLastOctet =
        static_cast<unsigned>(encoded.encodedBits % 8U);

    if (usedBitsInLastOctet != 0U) {
        const unsigned paddingBits = 8U - usedBitsInLastOctet;
        const auto paddingMask =
            static_cast<std::uint8_t>((1U << paddingBits) - 1U);

        auto nonZeroPadding = encoded.bytes;
        const bool originalPaddingIsZero =
            (nonZeroPadding.back() & paddingMask) == 0U;

        Check("ZERO_UPER_PADDING_CONFIRMED", originalPaddingIsZero);

        nonZeroPadding.back() |= 0x01U;

        Check("NONZERO_PADDING_FIXTURE_CREATED",
              nonZeroPadding != encoded.bytes);

        if (originalPaddingIsZero) {
            CheckDecodeFailure(
                "NONZERO_UPER_PADDING_REJECTED",
                nonZeroPadding);
        }
    } else {
        Check("NONZERO_PADDING_TEST_REQUIRES_UNALIGNED_FIXTURE", false);
    }

    const auto wrongVersion = ReencodeWithHeaderMutation(
        encoded.bytes,
        [](AsnRoot& root) {
            root.header.protocolVersion = 1;
        });

    Check("WRONG_VERSION_FIXTURE_CREATED",
          !wrongVersion.empty());

    if (!wrongVersion.empty()) {
        CheckDecodeFailure(
            "INCOMPATIBLE_PROTOCOL_VERSION_REJECTED",
            wrongVersion);
    }

    const auto wrongMessageId = ReencodeWithHeaderMutation(
        encoded.bytes,
        [](AsnRoot& root) {
            root.header.messageId = Vanetza_ITS2_MessageId_cam;
        });

    Check("WRONG_MESSAGE_ID_FIXTURE_CREATED",
          !wrongMessageId.empty());

    if (!wrongMessageId.empty()) {
        CheckDecodeFailure(
            "INCOMPATIBLE_MESSAGE_ID_REJECTED",
            wrongMessageId);
    }


    const auto longitudeValueNotUsed = ReencodeWithHeaderMutation(
        encoded.bytes,
        [](AsnRoot& root) {
            root.payload.managementContainer.referencePosition.longitude =
                Vanetza_ITS2_Longitude_valueNotUsed;
        });

    Check("LONGITUDE_VALUE_NOT_USED_FIXTURE_CREATED",
          !longitudeValueNotUsed.empty() &&
          longitudeValueNotUsed != encoded.bytes);

    if (!longitudeValueNotUsed.empty() &&
        longitudeValueNotUsed != encoded.bytes) {
        CheckDecodeFailure(
            "LONGITUDE_VALUE_NOT_USED_REJECTED",
            longitudeValueNotUsed);
    }

    const auto outOfRangeCoordinate = ReencodeFirstObjectMutation(
        encoded.bytes,
        [](Vanetza_ITS2_PerceivedObject_t& object) {
            object.position.xCoordinate.value =
                Vanetza_ITS2_CartesianCoordinateLarge_positiveOutOfRange;
        });

    Check("OUT_OF_RANGE_COORDINATE_FIXTURE_CREATED",
          !outOfRangeCoordinate.empty() &&
          outOfRangeCoordinate != encoded.bytes);

    if (!outOfRangeCoordinate.empty() &&
        outOfRangeCoordinate != encoded.bytes) {
        CheckDecodeFailure(
            "OUT_OF_RANGE_CARTESIAN_COORDINATE_REJECTED",
            outOfRangeCoordinate);
    }

    const auto missingObjectId = ReencodeFirstObjectMutation(
        encoded.bytes,
        [](Vanetza_ITS2_PerceivedObject_t& object) {
            std::free(object.objectId);
            object.objectId = nullptr;
        });

    Check("MISSING_OBJECT_ID_FIXTURE_CREATED",
          !missingObjectId.empty() &&
          missingObjectId != encoded.bytes);

    if (!missingObjectId.empty() &&
        missingObjectId != encoded.bytes) {
        CheckDecodeFailure(
            "MISSING_SEMANTIC_OBJECT_ID_REJECTED",
            missingObjectId);
    }

    const auto unsupportedObjectOptional = ReencodeFirstObjectMutation(
        encoded.bytes,
        [](Vanetza_ITS2_PerceivedObject_t& object) {
            object.objectPerceptionQuality =
                static_cast<Vanetza_ITS2_ObjectPerceptionQuality_t*>(
                    std::calloc(
                        1,
                        sizeof(Vanetza_ITS2_ObjectPerceptionQuality_t)));

            if (object.objectPerceptionQuality) {
                *object.objectPerceptionQuality = 12;
            }
        });

    Check("UNSUPPORTED_OPTIONAL_FIXTURE_CREATED",
          !unsupportedObjectOptional.empty() &&
          unsupportedObjectOptional != encoded.bytes);

    if (!unsupportedObjectOptional.empty() &&
        unsupportedObjectOptional != encoded.bytes) {
        CheckDecodeFailure(
            "UNSUPPORTED_OBJECT_OPTIONAL_REJECTED",
            unsupportedObjectOptional);
    }

    const auto duplicateRsuContainer = ReencodeWithHeaderMutation(
        encoded.bytes,
        [](AsnRoot& root) {
            auto* duplicate =
                static_cast<Vanetza_ITS2_WrappedCpmContainer_t*>(
                    std::calloc(
                        1,
                        sizeof(Vanetza_ITS2_WrappedCpmContainer_t)));

            if (!duplicate) {
                return;
            }

            duplicate->containerId =
                Vanetza_ITS2_CpmContainerId_originatingRsuContainer;
            duplicate->containerData.present =
                Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingRsuContainer;

            if (ASN_SEQUENCE_ADD(
                    &root.payload.cpmContainers.list,
                    duplicate) != 0) {
                std::free(duplicate);
            }
        });

    Check("DUPLICATE_RSU_CONTAINER_FIXTURE_CREATED",
          !duplicateRsuContainer.empty() &&
          duplicateRsuContainer != encoded.bytes);

    if (!duplicateRsuContainer.empty() &&
        duplicateRsuContainer != encoded.bytes) {
        CheckDecodeFailure(
            "DUPLICATE_RSU_CONTAINER_REJECTED",
            duplicateRsuContainer);
    }

    // Produzir um PDU estruturalmente válido com dois objetos incluídos.
    // Depois declarar uma contagem 1..255 válida em ASN.1, mas inferior
    // à cardinalidade da lista, para exercitar a rejeição semântica.
    CpmInput twoObjectInput = MakeValidInput();
    CpmPerceivedObject secondObject{};
    secondObject.objectId = 502;
    secondObject.measurementDeltaTimeMs = -8;
    secondObject.position.xEast.value = 20.0;
    secondObject.position.yNorth.value = 5.0;
    secondObject.objectAgeMs = 250;
    twoObjectInput.perceivedObjects.push_back(secondObject);

    const auto twoObjectEncoded = CpmUperEncoder::Encode(twoObjectInput);
    Check("TWO_OBJECT_FIXTURE_ENCODED",
          twoObjectEncoded.success && !twoObjectEncoded.bytes.empty());

    std::vector<std::uint8_t> countBelowIncluded;
    if (twoObjectEncoded.success && !twoObjectEncoded.bytes.empty())
    {
        countBelowIncluded = ReencodeWithHeaderMutation(
            twoObjectEncoded.bytes,
            [](AsnRoot& root) {
                auto& containers = root.payload.cpmContainers.list;
                for (int index = 0; index < containers.count; ++index)
                {
                    auto* container = containers.array[index];
                    if (container &&
                        container->containerId ==
                            Vanetza_ITS2_CpmContainerId_perceivedObjectContainer &&
                        container->containerData.present ==
                            Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer)
                    {
                        container->containerData.choice.PerceivedObjectContainer
                            .numberOfPerceivedObjects = 1;
                        return;
                    }
                }
            });
    }

    Check("COUNT_BELOW_INCLUDED_FIXTURE_CREATED",
          !countBelowIncluded.empty() &&
          countBelowIncluded != twoObjectEncoded.bytes);
    if (!countBelowIncluded.empty() &&
        countBelowIncluded != twoObjectEncoded.bytes)
    {
        CheckDecodeFailure(
            "COUNT_BELOW_INCLUDED_REJECTED",
            countBelowIncluded);
    }

    const auto missingRsuContainer = ReencodeWithoutContainer(
        encoded.bytes,
        static_cast<long>(
            Vanetza_ITS2_CpmContainerId_originatingRsuContainer));
    Check("MISSING_RSU_CONTAINER_FIXTURE_CREATED",
          !missingRsuContainer.empty() &&
          missingRsuContainer != encoded.bytes);
    if (!missingRsuContainer.empty() &&
        missingRsuContainer != encoded.bytes)
    {
        CheckDecodeFailure(
            "MISSING_RSU_CONTAINER_REJECTED",
            missingRsuContainer);
    }

    const auto missingPerceivedContainer = ReencodeWithoutContainer(
        encoded.bytes,
        static_cast<long>(
            Vanetza_ITS2_CpmContainerId_perceivedObjectContainer));
    Check("MISSING_OBJECT_CONTAINER_FIXTURE_CREATED",
          !missingPerceivedContainer.empty() &&
          missingPerceivedContainer != encoded.bytes);
    if (!missingPerceivedContainer.empty() &&
        missingPerceivedContainer != encoded.bytes)
    {
        CheckDecodeFailure(
            "MISSING_OBJECT_CONTAINER_REJECTED",
            missingPerceivedContainer);
    }

    const auto unsupportedContainer = ReencodeWithHeaderMutation(
        encoded.bytes,
        [](AsnRoot& root) {
            auto& containers = root.payload.cpmContainers.list;
            for (int index = 0; index < containers.count; ++index)
            {
                auto* container = containers.array[index];
                if (container &&
                    container->containerId ==
                        Vanetza_ITS2_CpmContainerId_originatingRsuContainer)
                {
                    // ID 6 está dentro do intervalo CpmContainerId (1..16),
                    // mas não pertence ao perfil implementado.
                    container->containerId =
                        static_cast<decltype(container->containerId)>(6);
                    return;
                }
            }
        });
    Check("UNSUPPORTED_CONTAINER_FIXTURE_CREATED",
          !unsupportedContainer.empty() &&
          unsupportedContainer != encoded.bytes);
    if (!unsupportedContainer.empty() &&
        unsupportedContainer != encoded.bytes)
    {
        CheckDecodeFailure(
            "UNSUPPORTED_CONTAINER_REJECTED",
            unsupportedContainer);
    }

    const auto inconsistentChoice = ReencodeWithHeaderMutation(
        encoded.bytes,
        [](AsnRoot& root) {
            if (!RemoveContainerById(
                    root,
                    static_cast<long>(
                        Vanetza_ITS2_CpmContainerId_originatingRsuContainer)))
            {
                return;
            }

            auto& containers = root.payload.cpmContainers.list;
            if (containers.count == 1 && containers.array[0])
            {
                // O contentor restante mantém o CHOICE PerceivedObject,
                // mas o identificador declara Originating RSU.
                containers.array[0]->containerId =
                    Vanetza_ITS2_CpmContainerId_originatingRsuContainer;
            }
        });
    Check("INCONSISTENT_CHOICE_FIXTURE_CREATED",
          !inconsistentChoice.empty() &&
          inconsistentChoice != encoded.bytes);
    if (!inconsistentChoice.empty() &&
        inconsistentChoice != encoded.bytes)
    {
        CheckDecodeFailure(
            "INCONSISTENT_CHOICE_REJECTED",
            inconsistentChoice);
    }

    const auto duplicatePerceivedContainer = ReencodeWithHeaderMutation(
        encoded.bytes,
        [](AsnRoot& root) {
            auto* duplicate =
                static_cast<Vanetza_ITS2_WrappedCpmContainer_t*>(
                    std::calloc(
                        1,
                        sizeof(Vanetza_ITS2_WrappedCpmContainer_t)));
            if (!duplicate)
            {
                return;
            }

            duplicate->containerId =
                Vanetza_ITS2_CpmContainerId_perceivedObjectContainer;
            duplicate->containerData.present =
                Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer;
            // Manter o contentor válido em ASN.1 para testar
            // especificamente a deteção de contentores duplicados.
            duplicate->containerData.choice.PerceivedObjectContainer
                .numberOfPerceivedObjects = 1;

            if (ASN_SEQUENCE_ADD(
                    &root.payload.cpmContainers.list,
                    duplicate) != 0)
            {
                ASN_STRUCT_FREE(
                    asn_DEF_Vanetza_ITS2_WrappedCpmContainer,
                    duplicate);
            }
        });
    Check("DUPLICATE_OBJECT_CONTAINER_FIXTURE_CREATED",
          !duplicatePerceivedContainer.empty() &&
          duplicatePerceivedContainer != encoded.bytes);
    if (!duplicatePerceivedContainer.empty() &&
        duplicatePerceivedContainer != encoded.bytes)
    {
        const auto duplicateResult =
            CpmUperDecoder::Decode(duplicatePerceivedContainer, 987654321);

        Check(
            "DUPLICATE_OBJECT_CONTAINER_REJECTED",
            !duplicateResult.success &&
                !duplicateResult.message.has_value() &&
                duplicateResult.error.find(
                    "perceived-object container is duplicated") !=
                    std::string::npos);
    }

    std::cout << "CPM_UPER_DECODER_CASES="
              << passed + failed << '\n';
    std::cout << "CPM_UPER_DECODER_PASSED="
              << passed << '\n';
    std::cout << "CPM_UPER_DECODER_FAILED="
              << failed << '\n';
    std::cout << "CPM_UPER_DECODER_CONTRACT="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
