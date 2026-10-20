#include <cstdlib>
#include <exception>
#include <iostream>

#include <vanetza/asn1/cpm.hpp>
#include <vanetza/asn1/support/INTEGER.h>
#include <vanetza/asn1/support/asn_SEQUENCE_OF.h>
#include <vanetza/asn1/support/asn_application.h>
#include <vanetza/asn1/its/r2/AltitudeConfidence.h>
#include <vanetza/asn1/its/r2/MessageId.h>
#include <vanetza/asn1/its/r2/WrappedCpmContainer.h>

int main()
{
    using vanetza::asn1::r2::Cpm;

    std::cout << "===== CPM R2 ROUND-TRIP PROBE =====\n";

    Cpm encoder;
    auto* msg = encoder.operator->();

    // ItsPduHeader: protocolVersion = 2, messageId = cpm.
    msg->header.protocolVersion = 2;
    msg->header.messageId = Vanetza_ITS2_MessageId_cpm;
    msg->header.stationId = 9001;

    // TimestampIts is INTEGER_t, not a scalar long.
    if (asn_long2INTEGER(
            &msg->payload.managementContainer.referenceTime,
            123456L) != 0) {
        std::cerr << "FAIL: referenceTime initialization\n";
        return 10;
    }

    // Minimal ReferencePosition with explicit, in-range values.
    auto& pos = msg->payload.managementContainer.referencePosition;
    pos.latitude = 0;
    pos.longitude = 0;
    pos.positionConfidenceEllipse.semiMajorConfidence = 100;
    pos.positionConfidenceEllipse.semiMinorConfidence = 100;
    pos.positionConfidenceEllipse.semiMajorOrientation = 0;
    pos.altitude.altitudeValue = 0;
    pos.altitude.altitudeConfidence =
        Vanetza_ITS2_AltitudeConfidence_unavailable;

    // One originating RSU container. Optional mapReference remains absent.
    auto* container =
        static_cast<Vanetza_ITS2_WrappedCpmContainer_t*>(
            std::calloc(1, sizeof(Vanetza_ITS2_WrappedCpmContainer_t)));

    if (!container) {
        std::cerr << "FAIL: container allocation\n";
        return 11;
    }

    container->containerId =
        Vanetza_ITS2_CpmContainerId_originatingRsuContainer;
    container->containerData.present =
        Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingRsuContainer;

    if (ASN_SEQUENCE_ADD(
            &msg->payload.cpmContainers.list, container) != 0) {
        std::free(container);
        std::cerr << "FAIL: container insertion\n";
        return 12;
    }

    // Validate the complete ASN.1 structure before encoding.
    char errbuf[512] = {};
    std::size_t errlen = sizeof(errbuf);

    const int constraint_rc = asn_check_constraints(
        &asn_DEF_Vanetza_ITS2_CollectivePerceptionMessage,
        msg, errbuf, &errlen);

    if (constraint_rc != 0) {
        std::cerr << "FAIL: ASN.1 constraints: "
                  << (errbuf[0] ? errbuf : "no detail") << '\n';
        return 13;
    }

    std::cout << "CPM_ASN1_CONSTRAINTS=PASS\n";

    // UPER encode.
    vanetza::ByteBuffer encoded;

    try {
        encoded = encoder.encode();
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: UPER encode: " << ex.what() << '\n';
        return 20;
    }

    if (encoded.empty()) {
        std::cerr << "FAIL: encoder returned an empty buffer\n";
        return 21;
    }

    std::cout << "CPM_UPER_ENCODE=PASS\n";
    std::cout << "CPM_UPER_SIZE=" << encoded.size() << '\n';

    // Decode the encoded bytes with a separate R2 wrapper.
    Cpm decoder;

    if (!decoder.decode(encoded)) {
        std::cerr << "FAIL: valid encoded CPM was rejected\n";
        return 30;
    }

    const auto* decoded = decoder.operator->();

    long decoded_reference_time = -1;
    if (asn_INTEGER2long(
            &decoded->payload.managementContainer.referenceTime,
            &decoded_reference_time) != 0) {
        std::cerr << "FAIL: decoded referenceTime conversion\n";
        return 31;
    }

    if (decoded->header.protocolVersion != 2 ||
        decoded->header.messageId != Vanetza_ITS2_MessageId_cpm ||
        decoded->header.stationId != 9001 ||
        decoded_reference_time != 123456L) {
        std::cerr << "FAIL: decoded header/referenceTime mismatch\n";
        return 32;
    }

    const auto& decoded_pos =
        decoded->payload.managementContainer.referencePosition;

    if (decoded_pos.latitude != 0 ||
        decoded_pos.longitude != 0 ||
        decoded_pos.positionConfidenceEllipse.semiMajorConfidence != 100 ||
        decoded_pos.positionConfidenceEllipse.semiMinorConfidence != 100 ||
        decoded_pos.altitude.altitudeValue != 0 ||
        decoded_pos.altitude.altitudeConfidence !=
            Vanetza_ITS2_AltitudeConfidence_unavailable) {
        std::cerr << "FAIL: decoded ReferencePosition mismatch\n";
        return 33;
    }

    const auto& list = decoded->payload.cpmContainers.list;

    if (list.count != 1 || list.array == nullptr || list.array[0] == nullptr) {
        std::cerr << "FAIL: decoded container list mismatch\n";
        return 34;
    }

    const auto* decoded_container = list.array[0];

    if (decoded_container->containerId !=
            Vanetza_ITS2_CpmContainerId_originatingRsuContainer ||
        decoded_container->containerData.present !=
            Vanetza_ITS2_WrappedCpmContainer__containerData_PR_OriginatingRsuContainer) {
        std::cerr << "FAIL: decoded RSU container mismatch\n";
        return 35;
    }

    std::cout << "CPM_HEADER_VERSION="
              << decoded->header.protocolVersion << '\n';
    std::cout << "CPM_MESSAGE_ID="
              << decoded->header.messageId << '\n';
    std::cout << "CPM_STATION_ID="
              << decoded->header.stationId << '\n';
    std::cout << "CPM_REFERENCE_TIME="
              << decoded_reference_time << '\n';
    std::cout << "CPM_CONTAINER_COUNT=" << list.count << '\n';
    std::cout << "CPM_CONTAINER_ID="
              << decoded_container->containerId << '\n';
    std::cout << "CPM_VALID_ROUNDTRIP=PASS\n";

    // Negative case 1: empty payload.
    Cpm empty_decoder;
    const vanetza::ByteBuffer empty_payload;

    if (empty_decoder.decode(empty_payload)) {
        std::cerr << "FAIL: empty payload accepted\n";
        return 40;
    }

    std::cout << "CPM_EMPTY_PAYLOAD_REJECTED=PASS\n";

    // Negative case 2: remove one byte from a valid encoded CPM.
    if (encoded.size() < 2) {
        std::cerr << "FAIL: encoded CPM unexpectedly too short\n";
        return 41;
    }

    vanetza::ByteBuffer truncated = encoded;
    truncated.resize(truncated.size() - 1);

    Cpm truncated_decoder;
    if (truncated_decoder.decode(truncated)) {
        std::cerr << "FAIL: truncated valid CPM accepted\n";
        return 42;
    }

    std::cout << "CPM_TRUNCATED_PAYLOAD_REJECTED=PASS\n";
    std::cout << "CPM_R2_ROUNDTRIP_PROBE=PASS\n";
    return 0;
}
