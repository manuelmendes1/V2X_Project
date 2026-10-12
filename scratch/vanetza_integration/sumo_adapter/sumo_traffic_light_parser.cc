#include "sumo_traffic_light_parser.h"

#include <cmath>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace ns3 {

namespace {

bool
ParseUnsigned(
    const std::string& token,
    uint32_t& value)
{
    if (token.empty()) {
        return false;
    }

    errno = 0;

    char* end = nullptr;

    const unsigned long parsed =
        std::strtoul(token.c_str(), &end, 10);

    if (errno != 0 ||
        end == token.c_str() ||
        *end != '\0' ||
        parsed > std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

bool
ParseSigned(
    const std::string& token,
    int32_t& value)
{
    if (token.empty()) {
        return false;
    }

    errno = 0;

    char* end = nullptr;

    const long parsed =
        std::strtol(token.c_str(), &end, 10);

    if (errno != 0 ||
        end == token.c_str() ||
        *end != '\0' ||
        parsed < std::numeric_limits<int32_t>::min() ||
        parsed > std::numeric_limits<int32_t>::max()) {
        return false;
    }

    value = static_cast<int32_t>(parsed);
    return true;
}

bool
ParseNonNegativeDouble(
    const std::string& token,
    double& value)
{
    if (token.empty()) {
        return false;
    }

    errno = 0;

    char* end = nullptr;

    const double parsed =
        std::strtod(token.c_str(), &end);

    if (errno != 0 ||
        end == token.c_str() ||
        *end != '\0' ||
        !std::isfinite(parsed) ||
        parsed < 0.0) {
        return false;
    }

    value = parsed;
    return true;
}

bool
IsValidRawState(const std::string& token)
{
    if (token.size() != 1) {
        return false;
    }

    const unsigned char c =
        static_cast<unsigned char>(token[0]);

    return !std::isspace(c);
}

} // namespace

std::unique_ptr<SumoTrafficLightState>
SumoTrafficLightParser::Parse(const std::string& message)
{
    std::stringstream ss(message);
    std::string token;

    auto state =
        std::make_unique<SumoTrafficLightState>();

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (token != "SUMO_TLS") {
        return nullptr;
    }

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (token.empty()) {
        return nullptr;
    }

    state->trafficLightId = token;

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (!ParseUnsigned(token, state->linkIndex)) {
        return nullptr;
    }

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (!IsValidRawState(token)) {
        return nullptr;
    }

    state->rawState = token[0];

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (!ParseUnsigned(token, state->phaseIndex)) {
        return nullptr;
    }

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (!ParseNonNegativeDouble(token, state->phaseDuration)) {
        return nullptr;
    }

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (!ParseNonNegativeDouble(token, state->nextSwitch)) {
        return nullptr;
    }

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (!ParseSigned(token, state->programId)) {
        return nullptr;
    }

    if (!std::getline(ss, token, ',')) {
        return nullptr;
    }

    if (!ParseNonNegativeDouble(token, state->simulationTime)) {
        return nullptr;
    }

    // A valid record contains exactly the 9 fields of the contract.
    if (std::getline(ss, token, ',')) {
        return nullptr;
    }

    return state;
}

} // namespace ns3
