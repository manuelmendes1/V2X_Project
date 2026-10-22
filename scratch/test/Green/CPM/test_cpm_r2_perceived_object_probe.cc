#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include <vanetza/asn1/cpm.hpp>
#include <vanetza/asn1/support/INTEGER.h>
#include <vanetza/asn1/support/asn_SEQUENCE_OF.h>
#include <vanetza/asn1/support/asn_application.h>
#include <vanetza/asn1/its/r2/AltitudeConfidence.h>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/WrappedCpmContainer.h>
#include <vanetza/asn1/its/r2/PerceivedObjects.h>

using vanetza::asn1::r2::Cpm;
using Root = Vanetza_ITS2_CollectivePerceptionMessage_t;
using Container = Vanetza_ITS2_WrappedCpmContainer_t;
using Object = Vanetza_ITS2_PerceivedObject_t;
using ObjectContainer = Vanetza_ITS2_PerceivedObjectContainer_t;

namespace
{

void FreeObject(Object* object)
{
    if (!object) {
        return;
    }

    std::free(object->objectId);
    std::free(object->objectAge);
    std::free(object->objectPerceptionQuality);
    std::free(object);
}

bool AddVehicleContainer(Cpm& cpm)
{
    auto* container =
        static_cast<Container*>(std::calloc(1, sizeof(Container)));

    if (!container) {
        return false;
    }

    container->containerId =
        Vanetza_ITS2_CpmContainerId_originatingVehicleContainer;
    container->containerData.present =
        Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingVehicleContainer;

    // Valor sintético válido: orientação alinhada com o norte de referência.
    container->containerData.choice.OriginatingVehicleContainer.orientationAngle.value = 0;
    container->containerData.choice.OriginatingVehicleContainer.orientationAngle.confidence = 127;

    if (ASN_SEQUENCE_ADD(
            &cpm->payload.cpmContainers.list, container) != 0) {
        std::free(container);
        return false;
    }

    return true;
}


bool InitializeBase(Cpm& cpm)
{
    auto* message = cpm.operator->();

    message->header.protocolVersion = 2;
    message->header.messageId = Vanetza_ITS2_MessageId_cpm;
    message->header.stationId = 9001;

    if (asn_long2INTEGER(
            &message->payload.managementContainer.referenceTime,
            123456L) != 0) {
        return false;
    }

    auto& position =
        message->payload.managementContainer.referencePosition;

    position.latitude = 0;
    position.longitude = 0;
    position.positionConfidenceEllipse.semiMajorConfidence = 100;
    position.positionConfidenceEllipse.semiMinorConfidence = 100;
    position.positionConfidenceEllipse.semiMajorOrientation = 0;
    position.altitude.altitudeValue = 0;
    position.altitude.altitudeConfidence =
        Vanetza_ITS2_AltitudeConfidence_unavailable;

    return AddVehicleContainer(cpm);
}

bool AddPerceivedObjectContainer(
    Cpm& cpm,
    bool includeObjectId,
    long declaredCount)
{
    auto* wrapper =
        static_cast<Container*>(std::calloc(1, sizeof(Container)));

    auto* object =
        static_cast<Object*>(std::calloc(1, sizeof(Object)));

    if (!wrapper || !object) {
        std::free(wrapper);
        std::free(object);
        return false;
    }

    wrapper->containerId =
        Vanetza_ITS2_CpmContainerId_perceivedObjectContainer;
    wrapper->containerData.present =
        Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer;

    auto& objectContainer =
        wrapper->containerData.choice.PerceivedObjectContainer;

    objectContainer.numberOfPerceivedObjects = declaredCount;

    // The selected test object uses a stable ID, a valid measurement
    // delta and explicit Cartesian position/confidence values.
    if (includeObjectId) {
        object->objectId =
            static_cast<Vanetza_ITS2_Identifier2B_t*>(
                std::calloc(1, sizeof(Vanetza_ITS2_Identifier2B_t)));

        if (!object->objectId) {
            FreeObject(object);
            std::free(wrapper);
            return false;
        }

        *object->objectId = 501;
    }

    object->measurementDeltaTime = -12;

    object->position.xCoordinate.value = 1200;
    object->position.xCoordinate.confidence = 100;

    object->position.yCoordinate.value = -350;
    object->position.yCoordinate.confidence = 200;

    // Exercise one optional field, while leaving the other optional
    // perceived-object fields absent.
    object->objectPerceptionQuality =
        static_cast<Vanetza_ITS2_ObjectPerceptionQuality_t*>(
            std::calloc(
                1, sizeof(Vanetza_ITS2_ObjectPerceptionQuality_t)));

    if (!object->objectPerceptionQuality) {
        FreeObject(object);
        std::free(wrapper);
        return false;
    }

    *object->objectPerceptionQuality = 12;

    if (ASN_SEQUENCE_ADD(
            &objectContainer.perceivedObjects.list, object) != 0) {
        FreeObject(object);
        std::free(objectContainer.perceivedObjects.list.array);
        std::free(wrapper);
        return false;
    }

    if (ASN_SEQUENCE_ADD(
            &cpm->payload.cpmContainers.list, wrapper) != 0) {
        FreeObject(object);
        std::free(objectContainer.perceivedObjects.list.array);
        std::free(wrapper);
        return false;
    }

    return true;
}

bool BuildMessage(
    Cpm& cpm,
    bool includeObjectId,
    long declaredCount)
{
    return InitializeBase(cpm) &&
           AddPerceivedObjectContainer(
               cpm, includeObjectId, declaredCount);
}

ObjectContainer* FindObjectContainer(Root* message)
{
    auto& list = message->payload.cpmContainers.list;

    for (int i = 0; i < list.count; ++i) {
        auto* container = list.array[i];

        if (container &&
            container->containerId ==
                Vanetza_ITS2_CpmContainerId_perceivedObjectContainer &&
            container->containerData.present ==
                Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer) {
            return &container->containerData.choice.PerceivedObjectContainer;
        }
    }

    return nullptr;
}

const ObjectContainer* FindObjectContainer(const Root* message)
{
    const auto& list = message->payload.cpmContainers.list;

    for (int i = 0; i < list.count; ++i) {
        const auto* container = list.array[i];

        if (container &&
            container->containerId ==
                Vanetza_ITS2_CpmContainerId_perceivedObjectContainer &&
            container->containerData.present ==
                Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer) {
            return &container->containerData.choice.PerceivedObjectContainer;
        }
    }

    return nullptr;
}

bool ObjectCountConsistent(const ObjectContainer& container)
{
    const long declaredCount = container.numberOfPerceivedObjects;
    const auto& objects = container.perceivedObjects.list;
    const long includedCount = objects.count;

    return declaredCount >= 0 &&
           declaredCount <= 255 &&
           includedCount >= 0 &&
           includedCount <= 255 &&
           declaredCount >= includedCount &&
           (includedCount == 0 || objects.array != nullptr);
}


bool ValidateSemanticInvariants(const Root* message, std::string& error)
{
    if (!message) {
        error = "null CPM message";
        return false;
    }

    if (message->header.protocolVersion != 2 ||
        message->header.messageId != Vanetza_ITS2_MessageId_cpm) {
        error = "unsupported CPM protocol version or message ID";
        return false;
    }

    const auto& containers = message->payload.cpmContainers.list;

    if (containers.count < 1 || containers.count > 8 ||
        containers.array == nullptr) {
        error = "CPM container count outside 1..8 or missing array";
        return false;
    }

    for (int i = 0; i < containers.count; ++i) {
        const auto* wrapped = containers.array[i];

        if (!wrapped) {
            error = "null CPM container entry";
            return false;
        }

        if (wrapped->containerId !=
            Vanetza_ITS2_CpmContainerId_perceivedObjectContainer) {
            continue;
        }

        if (wrapped->containerData.present !=
            Vanetza_ITS2_WrappedCpmContainer__containerData_PR_PerceivedObjectContainer) {
            error = "container ID and CHOICE type disagree";
            return false;
        }

        const auto& objectContainer =
            wrapped->containerData.choice.PerceivedObjectContainer;

        if (!ObjectCountConsistent(objectContainer)) {
            error =
                "numberOfPerceivedObjects is below included object count or cardinality is invalid";
            return false;
        }

        const auto& objects = objectContainer.perceivedObjects.list;

        if (objects.count > 255 ||
            (objects.count > 0 && objects.array == nullptr)) {
            error = "invalid perceived-object list cardinality or array";
            return false;
        }

        for (int j = 0; j < objects.count; ++j) {
            const auto* object = objects.array[j];

            if (!object) {
                error = "null PerceivedObject entry";
                return false;
            }

            if (!object->objectId) {
                error = "PerceivedObject.objectId must be present";
                return false;
            }

            if (*object->objectId < 0 || *object->objectId > 65535) {
                error = "PerceivedObject.objectId outside 0..65535";
                return false;
            }

            if (object->measurementDeltaTime < -2048 ||
                object->measurementDeltaTime > 2047) {
                error = "measurementDeltaTime outside -2048..2047";
                return false;
            }

            const auto& position = object->position;
            const auto coordinateValid = [](long value) {
                return value >= -131072 && value <= 131071;
            };
            const auto confidenceValid = [](long value) {
                return value >= 1 && value <= 4096;
            };

            if (!coordinateValid(position.xCoordinate.value) ||
                !coordinateValid(position.yCoordinate.value) ||
                !confidenceValid(position.xCoordinate.confidence) ||
                !confidenceValid(position.yCoordinate.confidence)) {
                error = "invalid Cartesian coordinate or confidence";
                return false;
            }

            if (position.zCoordinate &&
                (!coordinateValid(position.zCoordinate->value) ||
                 !confidenceValid(position.zCoordinate->confidence))) {
                error = "invalid optional Z coordinate or confidence";
                return false;
            }

            if (object->objectPerceptionQuality &&
                (*object->objectPerceptionQuality < 0 ||
                 *object->objectPerceptionQuality > 15)) {
                error = "objectPerceptionQuality outside 0..15";
                return false;
            }

            if (object->objectAge &&
                (*object->objectAge < 0 || *object->objectAge > 1500)) {
                error = "objectAge outside 0..1500";
                return false;
            }
        }
    }

    return true;
}

int CheckConstraints(Cpm& cpm, std::string& error)
{
    char buffer[512] = {};
    std::size_t length = sizeof(buffer);

    const int rc = asn_check_constraints(
        &asn_DEF_Vanetza_ITS2_CollectivePerceptionMessage,
        cpm.operator->(), buffer, &length);

    error = buffer;
    return rc;
}


template <typename Mutator>
bool CheckBoundaryCase(
    const char* name,
    Mutator mutate,
    bool expectedValid)
{
    Cpm cpm;

    if (!BuildMessage(cpm, true, 1)) {
        std::cerr << "FAIL: fixture construction for " << name << '\n';
        return false;
    }

    Object* object = nullptr;
    auto* container = FindObjectContainer(cpm.operator->());

    if (container &&
        container->perceivedObjects.list.count == 1 &&
        container->perceivedObjects.list.array) {
        object = container->perceivedObjects.list.array[0];
    }

    if (!object) {
        std::cerr << "FAIL: missing fixture object for " << name << '\n';
        return false;
    }

    mutate(*object);

    std::string semanticError;
    const bool semanticValid =
        ValidateSemanticInvariants(cpm.operator->(), semanticError);

    std::string asnError;
    const int asnRc = CheckConstraints(cpm, asnError);

    const bool passed = expectedValid
        ? semanticValid && asnRc == 0
        : !semanticValid;

    std::cout << "CPM_BOUNDARY_" << name << '='
              << (passed ? "PASS" : "FAIL")
              << " semantic_valid=" << semanticValid
              << " asn_rc=" << asnRc << '\n';

    if (!passed) {
        std::cerr << "Boundary failure " << name
                  << ": semantic_error=\"" << semanticError
                  << "\" asn_error=\"" << asnError << "\"\n";
    }

    return passed;
}


bool CheckObjectAgeBoundaryCase(
    const char* name,
    long age,
    bool expectedValid)
{
    Cpm cpm;

    if (!BuildMessage(cpm, true, 1)) {
        std::cerr << "FAIL: fixture construction for " << name << '\n';
        return false;
    }

    Object* object = nullptr;
    auto* container = FindObjectContainer(cpm.operator->());

    if (container &&
        container->perceivedObjects.list.count == 1 &&
        container->perceivedObjects.list.array) {
        object = container->perceivedObjects.list.array[0];
    }

    if (!object) {
        std::cerr << "FAIL: missing fixture object for " << name << '\n';
        return false;
    }

    object->objectAge =
        static_cast<Vanetza_ITS2_DeltaTimeMilliSecondSigned_t*>(
            std::calloc(
                1, sizeof(Vanetza_ITS2_DeltaTimeMilliSecondSigned_t)));

    if (!object->objectAge) {
        std::cerr << "FAIL: allocation of objectAge for " << name << '\n';
        return false;
    }

    *object->objectAge = age;

    std::string semanticError;
    const bool semanticValid =
        ValidateSemanticInvariants(cpm.operator->(), semanticError);

    std::string asnError;
    const int asnRc = CheckConstraints(cpm, asnError);

    const bool passed = expectedValid
        ? semanticValid && asnRc == 0
        : !semanticValid;

    std::cout << "CPM_BOUNDARY_" << name << '='
              << (passed ? "PASS" : "FAIL")
              << " age=" << age
              << " semantic_valid=" << semanticValid
              << " asn_rc=" << asnRc << '\n';

    if (!passed) {
        std::cerr << "Boundary failure " << name
                  << ": semantic_error=\"" << semanticError
                  << "\" asn_error=\"" << asnError << "\"\n";
    }

    return passed;
}

bool CheckSemanticBoundaryMatrix()
{
    bool passed = true;

    // Valid endpoints of each declared ASN.1 domain.
    passed = CheckBoundaryCase("OBJECT_ID_MIN", [](Object& o) {
        *o.objectId = 0;
    }, true) && passed;

    passed = CheckBoundaryCase("OBJECT_ID_MAX", [](Object& o) {
        *o.objectId = 65535;
    }, true) && passed;

    passed = CheckBoundaryCase("X_COORDINATE_MIN", [](Object& o) {
        o.position.xCoordinate.value = -131072;
    }, true) && passed;

    passed = CheckBoundaryCase("X_COORDINATE_MAX", [](Object& o) {
        o.position.xCoordinate.value = 131071;
    }, true) && passed;

    passed = CheckBoundaryCase("Y_COORDINATE_MIN", [](Object& o) {
        o.position.yCoordinate.value = -131072;
    }, true) && passed;

    passed = CheckBoundaryCase("Y_COORDINATE_MAX", [](Object& o) {
        o.position.yCoordinate.value = 131071;
    }, true) && passed;

    passed = CheckBoundaryCase("CONFIDENCE_MIN", [](Object& o) {
        o.position.xCoordinate.confidence = 1;
    }, true) && passed;

    passed = CheckBoundaryCase("CONFIDENCE_MAX", [](Object& o) {
        o.position.yCoordinate.confidence = 4096;
    }, true) && passed;

    passed = CheckBoundaryCase("MEASUREMENT_DELTA_MIN", [](Object& o) {
        o.measurementDeltaTime = -2048;
    }, true) && passed;

    passed = CheckBoundaryCase("MEASUREMENT_DELTA_MAX", [](Object& o) {
        o.measurementDeltaTime = 2047;
    }, true) && passed;

    passed = CheckObjectAgeBoundaryCase("OBJECT_AGE_MIN", 0, true)
        && passed;
    passed = CheckObjectAgeBoundaryCase("OBJECT_AGE_MAX", 1500, true)
        && passed;
    passed = CheckObjectAgeBoundaryCase(
        "OBJECT_AGE_ABOVE_SPEC_MAX", 1501, false) && passed;

    passed = CheckBoundaryCase("QUALITY_MIN", [](Object& o) {
        *o.objectPerceptionQuality = 0;
    }, true) && passed;

    passed = CheckBoundaryCase("QUALITY_MAX", [](Object& o) {
        *o.objectPerceptionQuality = 15;
    }, true) && passed;

    // One value immediately outside each permitted interval.
    passed = CheckBoundaryCase("OBJECT_ID_BELOW_MIN", [](Object& o) {
        *o.objectId = -1;
    }, false) && passed;

    passed = CheckBoundaryCase("OBJECT_ID_ABOVE_MAX", [](Object& o) {
        *o.objectId = 65536;
    }, false) && passed;

    passed = CheckBoundaryCase("X_COORDINATE_BELOW_MIN", [](Object& o) {
        o.position.xCoordinate.value = -131073;
    }, false) && passed;

    passed = CheckBoundaryCase("X_COORDINATE_ABOVE_MAX", [](Object& o) {
        o.position.xCoordinate.value = 131072;
    }, false) && passed;

    passed = CheckBoundaryCase("Y_COORDINATE_BELOW_MIN", [](Object& o) {
        o.position.yCoordinate.value = -131073;
    }, false) && passed;

    passed = CheckBoundaryCase("Y_COORDINATE_ABOVE_MAX", [](Object& o) {
        o.position.yCoordinate.value = 131072;
    }, false) && passed;

    passed = CheckBoundaryCase("CONFIDENCE_BELOW_MIN", [](Object& o) {
        o.position.xCoordinate.confidence = 0;
    }, false) && passed;

    passed = CheckBoundaryCase("CONFIDENCE_ABOVE_MAX", [](Object& o) {
        o.position.yCoordinate.confidence = 4097;
    }, false) && passed;

    passed = CheckBoundaryCase("MEASUREMENT_DELTA_BELOW_MIN", [](Object& o) {
        o.measurementDeltaTime = -2049;
    }, false) && passed;

    passed = CheckBoundaryCase("MEASUREMENT_DELTA_ABOVE_MAX", [](Object& o) {
        o.measurementDeltaTime = 2048;
    }, false) && passed;

    passed = CheckObjectAgeBoundaryCase("OBJECT_AGE_BELOW_MIN", -1, false)
        && passed;
    passed = CheckObjectAgeBoundaryCase("OBJECT_AGE_ABOVE_MAX", 2048, false)
        && passed;

    passed = CheckBoundaryCase("QUALITY_BELOW_MIN", [](Object& o) {
        *o.objectPerceptionQuality = -1;
    }, false) && passed;

    passed = CheckBoundaryCase("QUALITY_ABOVE_MAX", [](Object& o) {
        *o.objectPerceptionQuality = 16;
    }, false) && passed;

    std::cout << "CPM_SEMANTIC_BOUNDARY_MATRIX="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed;
}

bool CheckValidRoundTrip()
{
    Cpm encoder;

    if (!BuildMessage(encoder, true, 1)) {
        std::cerr << "FAIL: CPM fixture construction\n";
        return false;
    }

    const auto* objectContainer =
        FindObjectContainer(encoder.operator->());

    if (!objectContainer ||
        !ObjectCountConsistent(*objectContainer)) {
        std::cerr << "FAIL: object count/list mismatch in valid fixture\n";
        return false;
    }

    std::string error;
    if (CheckConstraints(encoder, error) != 0) {
        std::cerr << "FAIL: ASN.1 constraints: " << error << '\n';
        return false;
    }

    std::cout << "CPM_OBJECT_ASN1_CONSTRAINTS=PASS\n";

    vanetza::ByteBuffer encoded;

    try {
        encoded = encoder.encode();
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: UPER encode: " << ex.what() << '\n';
        return false;
    }

    if (encoded.empty()) {
        std::cerr << "FAIL: empty UPER result\n";
        return false;
    }

    std::cout << "CPM_OBJECT_UPER_ENCODE=PASS\n";
    std::cout << "CPM_OBJECT_UPER_SIZE=" << encoded.size() << '\n';

    Cpm decoder;
    if (!decoder.decode(encoded)) {
        std::cerr << "FAIL: valid CPM object payload rejected\n";
        return false;
    }

    const Root* message = decoder.operator->();

    long referenceTime = -1;
    if (asn_INTEGER2long(
            &message->payload.managementContainer.referenceTime,
            &referenceTime) != 0) {
        std::cerr << "FAIL: reference time conversion\n";
        return false;
    }

    const auto& referencePosition =
        message->payload.managementContainer.referencePosition;

    const auto& decodedContainers = message->payload.cpmContainers.list;

    if (decodedContainers.count < 2 ||
        !decodedContainers.array ||
        !decodedContainers.array[0] ||
        decodedContainers.array[0]->containerId !=
            Vanetza_ITS2_CpmContainerId_originatingVehicleContainer ||
        decodedContainers.array[0]->containerData.present !=
            Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingVehicleContainer ||
        decodedContainers.array[0]->containerData.choice.OriginatingVehicleContainer.orientationAngle.value != 0 ||
        decodedContainers.array[0]->containerData.choice
            .OriginatingVehicleContainer.orientationAngle.confidence != 127) {
        std::cerr << "FAIL: decoded originating vehicle container/profile mismatch\n";
        return false;
    }

    std::cout << "CPM_OBJECT_ORIGIN_CONTAINER_ID="
              << decodedContainers.array[0]->containerId << '\n';
    std::cout << "CPM_OBJECT_ORIGIN_VEHICLE_ANGLE="
              << decodedContainers.array[0]->containerData.choice
                     .OriginatingVehicleContainer.orientationAngle.value << '\n';
    std::cout << "CPM_OBJECT_ORIGIN_VEHICLE_CONFIDENCE="
              << decodedContainers.array[0]->containerData.choice
                     .OriginatingVehicleContainer.orientationAngle.confidence << '\n';

    const auto* decodedContainer = FindObjectContainer(message);

    if (!decodedContainer ||
        message->header.protocolVersion != 2 ||
        message->header.messageId != Vanetza_ITS2_MessageId_cpm ||
        message->header.stationId != 9001 ||
        referenceTime != 123456 ||
        referencePosition.latitude != 0 ||
        referencePosition.longitude != 0 ||
        decodedContainer->numberOfPerceivedObjects != 1 ||
        decodedContainer->perceivedObjects.list.count != 1 ||
        !ObjectCountConsistent(*decodedContainer)) {
        std::cerr << "FAIL: decoded message/container invariant mismatch\n";
        return false;
    }

    const auto* list = &decodedContainer->perceivedObjects.list;

    if (!list->array || !list->array[0]) {
        std::cerr << "FAIL: decoded perceived object missing\n";
        return false;
    }

    const auto* object = list->array[0];

    if (!object->objectId ||
        *object->objectId != 501 ||
        object->measurementDeltaTime != -12 ||
        object->position.xCoordinate.value != 1200 ||
        object->position.xCoordinate.confidence != 100 ||
        object->position.yCoordinate.value != -350 ||
        object->position.yCoordinate.confidence != 200 ||
        !object->objectPerceptionQuality ||
        *object->objectPerceptionQuality != 12) {
        std::cerr << "FAIL: decoded perceived-object fields differ\n";
        return false;
    }

    std::cout << "CPM_OBJECT_ID=" << *object->objectId << '\n';
    std::cout << "CPM_OBJECT_MEASUREMENT_DELTA="
              << object->measurementDeltaTime << '\n';
    std::cout << "CPM_OBJECT_X=" << object->position.xCoordinate.value
              << '\n';
    std::cout << "CPM_OBJECT_Y=" << object->position.yCoordinate.value
              << '\n';
    std::cout << "CPM_OBJECT_QUALITY="
              << *object->objectPerceptionQuality << '\n';
    std::cout << "CPM_OBJECT_VALID_ROUNDTRIP=PASS\n";

    Cpm emptyDecoder;
    const vanetza::ByteBuffer emptyPayload;

    if (emptyDecoder.decode(emptyPayload)) {
        std::cerr << "FAIL: empty payload accepted\n";
        return false;
    }

    std::cout << "CPM_EMPTY_PAYLOAD_REJECTED=PASS\n";

    if (encoded.size() < 2) {
        std::cerr << "FAIL: unexpected encoded size\n";
        return false;
    }

    vanetza::ByteBuffer truncated = encoded;
    truncated.resize(truncated.size() - 1);

    Cpm truncatedDecoder;
    if (truncatedDecoder.decode(truncated)) {
        std::cerr << "FAIL: truncated payload accepted\n";
        return false;
    }

    std::cout << "CPM_TRUNCATED_PAYLOAD_REJECTED=PASS\n";
    return true;
}

bool CheckMissingObjectIdRejected()
{
    Cpm malformed;

    if (!BuildMessage(malformed, false, 1)) {
        std::cerr << "FAIL: missing-ID fixture construction\n";
        return false;
    }

    std::string rootError;
    const int rootRc = CheckConstraints(malformed, rootError);

    auto* objectContainer = FindObjectContainer(malformed.operator->());
    if (!objectContainer ||
        objectContainer->perceivedObjects.list.count != 1 ||
        !objectContainer->perceivedObjects.list.array ||
        !objectContainer->perceivedObjects.list.array[0]) {
        std::cerr << "FAIL: missing-ID fixture shape is incorrect\n";
        return false;
    }

    char listBuffer[512] = {};
    std::size_t listLength = sizeof(listBuffer);
    const int listRc = asn_check_constraints(
        &asn_DEF_Vanetza_ITS2_PerceivedObjects,
        &objectContainer->perceivedObjects,
        listBuffer,
        &listLength);

    std::cout << "CPM_MISSING_OBJECT_ID_ROOT_CONSTRAINT_RC="
              << rootRc << '\n';
    std::cout << "CPM_MISSING_OBJECT_ID_LIST_CONSTRAINT_RC="
              << listRc << '\n';

    if (rootRc == 0) {
        std::cout
            << "CPM_MISSING_OBJECT_ID_ROOT_CONSTRAINT_DID_NOT_REJECT=CONFIRMED\n";
    }

    if (listRc == 0) {
        std::cout
            << "CPM_MISSING_OBJECT_ID_LIST_CONSTRAINT_DID_NOT_REJECT=CONFIRMED\n";
    } else {
        std::cout
            << "CPM_MISSING_OBJECT_ID_LIST_CONSTRAINT_REJECTED=CONFIRMED\n";
    }

    std::string semanticError;
    if (ValidateSemanticInvariants(malformed.operator->(), semanticError)) {
        std::cerr
            << "FAIL: semantic guard accepted PerceivedObject without objectId\n";
        return false;
    }

    if (semanticError.find("objectId") == std::string::npos) {
        std::cerr
            << "FAIL: rejection was not caused by the objectId invariant: "
            << semanticError << '\n';
        return false;
    }

    std::cout << "CPM_MISSING_OBJECT_ID_REJECTED=PASS\n";
    std::cout << "CPM_MISSING_OBJECT_ID_REASON="
              << semanticError << '\n';
    return true;
}

bool CheckCountSemantics()
{
    // O emissor conhece dois objetos, mas inclui apenas um nesta mensagem.
    Cpm largerTotal;
    if (!BuildMessage(largerTotal, true, 2)) {
        std::cerr << "FAIL: larger-total fixture construction\n";
        return false;
    }

    auto* largerContainer = FindObjectContainer(largerTotal.operator->());
    std::string asnError;
    if (!largerContainer ||
        !ObjectCountConsistent(*largerContainer) ||
        CheckConstraints(largerTotal, asnError) != 0) {
        std::cerr << "FAIL: total greater than included count should be valid: "
                  << asnError << '\n';
        return false;
    }

    std::string semanticError;
    if (!ValidateSemanticInvariants(
            largerTotal.operator->(), semanticError)) {
        std::cerr << "FAIL: validator rejected a valid CPM subset: "
                  << semanticError << '\n';
        return false;
    }

    std::cout << "CPM_COUNT_TOTAL_GREATER_THAN_INCLUDED=PASS\n";

    // Um objeto é incluído, portanto o total percebido não pode ser zero.
    Cpm tooSmall;
    if (!BuildMessage(tooSmall, true, 0)) {
        std::cerr << "FAIL: too-small-total fixture construction\n";
        return false;
    }

    std::string badAsnError;
    const int badAsnRc = CheckConstraints(tooSmall, badAsnError);
    std::cout << "CPM_COUNT_BELOW_INCLUDED_ASN_CONSTRAINT_RC="
              << badAsnRc << '\n';

    if (badAsnRc == 0) {
        std::cout
            << "CPM_COUNT_BELOW_INCLUDED_NOT_COVERED_BY_ASN_CONSTRAINTS=CONFIRMED\n";
    }

    semanticError.clear();
    if (ValidateSemanticInvariants(
            tooSmall.operator->(), semanticError)) {
        std::cerr << "FAIL: semantic guard accepted total below included count\n";
        return false;
    }

    if (semanticError.find("numberOfPerceivedObjects") == std::string::npos) {
        std::cerr << "FAIL: unexpected count rejection reason: "
                  << semanticError << '\n';
        return false;
    }

    std::cout << "CPM_COUNT_BELOW_INCLUDED_SEMANTIC_GUARD=PASS\n";
    std::cout << "CPM_COUNT_BELOW_INCLUDED_REJECTED=PASS\n";
    std::cout << "CPM_COUNT_BELOW_INCLUDED_REASON="
              << semanticError << '\n';

    return true;
}


} // namespace

int main()
{
    std::cout << "===== CPM R2 PERCEIVED OBJECT PROBE =====\n";

    const bool roundTrip = CheckValidRoundTrip();
    const bool missingId = CheckMissingObjectIdRejected();
    const bool countSemantics = CheckCountSemantics();
    const bool boundaries = CheckSemanticBoundaryMatrix();

    std::cout << "CPM_R2_PERCEIVED_OBJECT_ROUNDTRIP="
              << (roundTrip ? "PASS" : "FAIL") << '\n';
    std::cout << "CPM_R2_MISSING_OBJECT_ID="
              << (missingId ? "PASS" : "FAIL") << '\n';
    std::cout << "CPM_R2_COUNT_SEMANTICS="
              << (countSemantics ? "PASS" : "FAIL") << '\n';
    std::cout << "CPM_R2_SEMANTIC_BOUNDARY_MATRIX="
              << (boundaries ? "PASS" : "FAIL") << '\n';

    const bool passed =
        roundTrip && missingId && countSemantics && boundaries;

    std::cout << "CPM_R2_PERCEIVED_OBJECT_PROBE="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed ? 0 : 1;
}
