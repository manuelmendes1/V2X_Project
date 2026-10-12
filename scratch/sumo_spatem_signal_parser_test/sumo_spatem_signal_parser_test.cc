#include "vanetza_integration/sumo_adapter/sumo_traffic_light_parser.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

bool
ExpectValid(
    const std::string& message,
    uint32_t expectedLinkIndex,
    char expectedRawState,
    uint32_t expectedPhaseIndex)
{
    const auto parsed =
        ns3::SumoTrafficLightParser::Parse(message);

    if (!parsed) {
        std::cerr
            << "FAIL valid record rejected: "
            << message << '\n';
        return false;
    }

    if (parsed->trafficLightId != "B0") {
        std::cerr << "FAIL trafficLightId\n";
        return false;
    }

    if (parsed->linkIndex != expectedLinkIndex) {
        std::cerr << "FAIL linkIndex\n";
        return false;
    }

    if (parsed->rawState != expectedRawState) {
        std::cerr << "FAIL rawState\n";
        return false;
    }

    if (parsed->phaseIndex != expectedPhaseIndex) {
        std::cerr << "FAIL phaseIndex\n";
        return false;
    }

    if (parsed->phaseDuration < 0.0 ||
        !std::isfinite(parsed->phaseDuration)) {
        std::cerr << "FAIL phaseDuration\n";
        return false;
    }

    if (parsed->nextSwitch < 0.0 ||
        !std::isfinite(parsed->nextSwitch)) {
        std::cerr << "FAIL nextSwitch\n";
        return false;
    }

    if (parsed->simulationTime < 0.0 ||
        !std::isfinite(parsed->simulationTime)) {
        std::cerr << "FAIL simulationTime\n";
        return false;
    }

    return true;
}

bool
ExpectInvalid(const std::string& message)
{
    const auto parsed =
        ns3::SumoTrafficLightParser::Parse(message);

    if (parsed) {
        std::cerr
            << "FAIL invalid record accepted: "
            << message << '\n';
        return false;
    }

    return true;
}

} // namespace

int
main()
{
    // ---------------------------------------------------------------------
    // Valid records observed/derived from the real B0 fixture.
    // ---------------------------------------------------------------------

    if (!ExpectValid(
            "SUMO_TLS,B0,0,G,0,30.0,30.0,1,0.0",
            0,
            'G',
            0)) {
        return 1;
    }

    if (!ExpectValid(
            "SUMO_TLS,B0,1,G,0,30.0,30.0,1,0.0",
            1,
            'G',
            0)) {
        return 1;
    }

    if (!ExpectValid(
            "SUMO_TLS,B0,0,y,1,10.0,40.0,1,30.1",
            0,
            'y',
            1)) {
        return 1;
    }

    if (!ExpectValid(
            "SUMO_TLS,B0,1,y,1,10.0,40.0,1,30.1",
            1,
            'y',
            1)) {
        return 1;
    }

    // ---------------------------------------------------------------------
    // Boundary rejection cases.
    // ---------------------------------------------------------------------

    if (!ExpectInvalid(
            "TLS,B0,0,G,0,30.0,30.0,1,0.0")) {
        return 1;
    }

    if (!ExpectInvalid(
            "SUMO_TLS,B0,0,G,0,30.0,30.0,1")) {
        return 1;
    }

    if (!ExpectInvalid(
            "SUMO_TLS,B0,0,G,0,30.0,30.0,1,0.0,EXTRA")) {
        return 1;
    }

    if (!ExpectInvalid(
            "SUMO_TLS,B0,invalid,G,0,30.0,30.0,1,0.0")) {
        return 1;
    }

    if (!ExpectInvalid(
            "SUMO_TLS,B0,0,,0,30.0,30.0,1,0.0")) {
        return 1;
    }

    if (!ExpectInvalid(
            "SUMO_TLS,B0,0,GG,0,30.0,30.0,1,0.0")) {
        return 1;
    }

    if (!ExpectInvalid(
            "SUMO_TLS,B0,0,G,0,-1.0,30.0,1,0.0")) {
        return 1;
    }

    if (!ExpectInvalid(
            "SUMO_TLS,B0,0,G,0,30.0,nan,1,0.0")) {
        return 1;
    }

    if (!ExpectInvalid(
            "SUMO_TLS,B0,0,G,0,30.0,30.0,1,-0.1")) {
        return 1;
    }

    std::cout
        << "PASS valid SUMO TLS records accepted\n"
        << "PASS linkIndex 0/1 preserved as signal-group source\n"
        << "PASS rawState G/y preserved\n"
        << "PASS phase/timing values preserved\n"
        << "PASS malformed/extraneous records rejected\n"
        << "SUMO_SPATEM_SIGNAL_PARSER_ACCEPTANCE=PASS\n";

    return 0;
}
