#include "vanetza_integration/sumo_adapter/sumo_network_projection_loader.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <charconv>
#include <cmath>
#include <memory>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ns3
{
namespace
{

bool Reject(std::string& error, const char* reason)
{
    error = reason;
    return false;
}

std::string_view Trim(std::string_view value)
{
    constexpr std::string_view whitespace = " \t\r\n";

    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos)
    {
        return {};
    }

    const auto last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

bool ParseFiniteDouble(std::string_view token, double& output)
{
    token = Trim(token);

    if (token.empty())
    {
        return false;
    }

    double candidate{};
    const char* first = token.data();
    const char* last = first + token.size();

    const auto parsed = std::from_chars(
        first,
        last,
        candidate,
        std::chars_format::general);

    if (parsed.ec != std::errc{} ||
        parsed.ptr != last ||
        !std::isfinite(candidate))
    {
        return false;
    }

    output = candidate;
    return true;
}

bool ParsePositiveInteger(std::string_view token, int& output)
{
    token = Trim(token);

    if (token.empty())
    {
        return false;
    }

    int candidate{};
    const char* first = token.data();
    const char* last = first + token.size();

    const auto parsed = std::from_chars(first, last, candidate, 10);

    if (parsed.ec != std::errc{} ||
        parsed.ptr != last ||
        candidate < 1 ||
        candidate > 60)
    {
        return false;
    }

    output = candidate;
    return true;
}

bool ParseNetOffset(
    std::string_view value,
    double& offsetX,
    double& offsetY)
{
    const auto comma = value.find(',');

    if (comma == std::string_view::npos ||
        value.find(',', comma + 1) != std::string_view::npos)
    {
        return false;
    }

    double candidateX{};
    double candidateY{};

    if (!ParseFiniteDouble(value.substr(0, comma), candidateX) ||
        !ParseFiniteDouble(value.substr(comma + 1), candidateY))
    {
        return false;
    }

    offsetX = candidateX;
    offsetY = candidateY;
    return true;
}

bool ParseProjection(
    const std::string& projection,
    SumoUtmWgs84ProjectionConfig& output,
    std::string& error)
{
    std::map<std::string, std::string> parameters;

    std::istringstream tokens(projection);
    std::string token;

    while (tokens >> token)
    {
        if (token.size() < 2 || token.front() != '+')
        {
            return Reject(
                error,
                "Malformed PROJ parameter token");
        }

        const std::string_view body(token.data() + 1, token.size() - 1);
        const auto equals = body.find('=');

        const std::string key(
            body.substr(0, equals));

        const std::string value =
            equals == std::string_view::npos
                ? std::string{}
                : std::string(body.substr(equals + 1));

        if (key.empty())
        {
            return Reject(
                error,
                "PROJ parameter name must not be empty");
        }

        if (!parameters.emplace(key, value).second)
        {
            return Reject(
                error,
                "Duplicate PROJ parameter");
        }
    }

    const auto projIt = parameters.find("proj");
    const auto zoneIt = parameters.find("zone");
    const auto datumIt = parameters.find("datum");
    const auto ellipsoidIt = parameters.find("ellps");
    const auto unitsIt = parameters.find("units");

    if (projIt == parameters.end() ||
        projIt->second != "utm")
    {
        return Reject(
            error,
            "Only +proj=utm is supported");
    }

    if (zoneIt == parameters.end())
    {
        return Reject(
            error,
            "UTM zone parameter is required");
    }

    int zone{};
    if (!ParsePositiveInteger(zoneIt->second, zone))
    {
        return Reject(
            error,
            "UTM zone must be within [1, 60]");
    }

    if (datumIt == parameters.end() ||
        datumIt->second != "WGS84")
    {
        return Reject(
            error,
            "PROJ datum must be explicitly WGS84");
    }

    if (ellipsoidIt != parameters.end() &&
        ellipsoidIt->second != "WGS84")
    {
        return Reject(
            error,
            "PROJ ellipsoid must be WGS84 when specified");
    }

    if (unitsIt == parameters.end() ||
        unitsIt->second != "m")
    {
        return Reject(
            error,
            "UTM projection units must be metres");
    }

    const auto southIt = parameters.find("south");
    const auto noDefsIt = parameters.find("no_defs");

    if (southIt != parameters.end() && !southIt->second.empty())
    {
        return Reject(
            error,
            "+south must be a flag without a value");
    }

    if (noDefsIt != parameters.end() && !noDefsIt->second.empty())
    {
        return Reject(
            error,
            "+no_defs must be a flag without a value");
    }

    // Strict allow-list: parameters that can alter the transformation
    // must not be silently ignored.
    for (const auto& [key, value] : parameters)
    {
        (void)value;

        if (key != "proj" &&
            key != "zone" &&
            key != "datum" &&
            key != "ellps" &&
            key != "units" &&
            key != "south" &&
            key != "no_defs")
        {
            return Reject(
                error,
                "Unsupported PROJ parameter");
        }
    }

    SumoUtmWgs84ProjectionConfig candidate{};
    candidate.zone = zone;
    candidate.northernHemisphere = southIt == parameters.end();

    output = candidate;
    return true;
}

bool ReadAttribute(
    xmlNode* node,
    const char* name,
    std::string& output)
{
    xmlChar* raw = xmlGetProp(node, BAD_CAST name);

    if (raw == nullptr)
    {
        return false;
    }

    output.assign(reinterpret_cast<const char*>(raw));
    xmlFree(raw);
    return !output.empty();
}

struct XmlDocumentDeleter
{
    void operator()(xmlDoc* document) const noexcept
    {
        if (document != nullptr)
        {
            xmlFreeDoc(document);
        }
    }
};

} // namespace

bool
SumoNetworkProjectionLoader::LoadUtmWgs84(
    const std::string& netFile,
    SumoUtmWgs84ProjectionConfig& output,
    std::string& error)
{
    error.clear();

    if (netFile.empty())
    {
        return Reject(error, "SUMO network file path is empty");
    }

    // Do not load network resources or substitute entities from external
    // sources. A DTD is rejected below as well.
    std::unique_ptr<xmlDoc, XmlDocumentDeleter> document(
        xmlReadFile(
            netFile.c_str(),
            nullptr,
            XML_PARSE_NONET |
                XML_PARSE_NOBLANKS |
                XML_PARSE_NOERROR |
                XML_PARSE_NOWARNING));

    if (!document)
    {
        return Reject(
            error,
            "Unable to parse SUMO network XML file");
    }

    if (document->intSubset != nullptr ||
        document->extSubset != nullptr)
    {
        return Reject(
            error,
            "SUMO network XML must not declare a DTD");
    }

    xmlNode* root = xmlDocGetRootElement(document.get());

    if (root == nullptr ||
        root->type != XML_ELEMENT_NODE ||
        !xmlStrEqual(root->name, BAD_CAST "net"))
    {
        return Reject(
            error,
            "SUMO network XML root element must be <net>");
    }

    xmlNode* location = nullptr;
    std::size_t locationCount = 0;

    for (xmlNode* child = root->children;
         child != nullptr;
         child = child->next)
    {
        if (child->type == XML_ELEMENT_NODE &&
            xmlStrEqual(child->name, BAD_CAST "location"))
        {
            location = child;
            ++locationCount;
        }
    }

    if (locationCount != 1 || location == nullptr)
    {
        return Reject(
            error,
            "SUMO network must contain exactly one direct <location> element");
    }

    std::string netOffsetText;
    std::string projectionText;

    if (!ReadAttribute(location, "netOffset", netOffsetText))
    {
        return Reject(
            error,
            "SUMO <location> netOffset attribute is required");
    }

    if (!ReadAttribute(location, "projParameter", projectionText))
    {
        return Reject(
            error,
            "SUMO <location> projParameter attribute is required");
    }

    double offsetX{};
    double offsetY{};

    if (!ParseNetOffset(netOffsetText, offsetX, offsetY))
    {
        return Reject(
            error,
            "SUMO netOffset must contain two finite comma-separated numbers");
    }

    SumoUtmWgs84ProjectionConfig candidate{};

    if (!ParseProjection(projectionText, candidate, error))
    {
        return false;
    }

    candidate.netOffsetXMeters = offsetX;
    candidate.netOffsetYMeters = offsetY;

    // Publish only after the entire XML and projection configuration
    // have passed validation.
    output = candidate;
    error.clear();
    return true;
}

} // namespace ns3
