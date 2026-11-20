#include "vanetza_integration/sumo_adapter/sumo_network_projection_loader.h"
#include "vanetza_integration/sumo_adapter/sumo_geodetic_transform.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace
{

bool Check(bool condition, const char* name, bool& passed)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << name << '\n';

    passed = passed && condition;
    return condition;
}

bool SameConfig(
    const ns3::SumoUtmWgs84ProjectionConfig& lhs,
    const ns3::SumoUtmWgs84ProjectionConfig& rhs)
{
    return lhs.zone == rhs.zone &&
           lhs.northernHemisphere == rhs.northernHemisphere &&
           lhs.netOffsetXMeters == rhs.netOffsetXMeters &&
           lhs.netOffsetYMeters == rhs.netOffsetYMeters;
}

bool WriteFile(
    const std::filesystem::path& path,
    const std::string& content)
{
    std::ofstream output(path);
    if (!output)
    {
        return false;
    }

    output << content;
    return output.good();
}

std::string NetWithLocation(const std::string& attributes)
{
    return "<?xml version=\"1.0\"?>\n"
           "<net>\n"
           "  <location " + attributes + "/>\n"
           "</net>\n";
}

bool ExpectRejected(
    const std::filesystem::path& path,
    const char* name,
    bool& passed)
{
    const ns3::SumoUtmWgs84ProjectionConfig sentinel{
        17, false, 123.5, 456.5
    };

    auto output = sentinel;
    std::string error = "stale";

    const bool result =
        ns3::SumoNetworkProjectionLoader::LoadUtmWgs84(
            path.string(), output, error);

    return Check(
        !result &&
        !error.empty() &&
        SameConfig(output, sentinel),
        name,
        passed);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Expected test fixture directory\n";
        return 2;
    }

    const std::filesystem::path directory(argv[1]);

    bool passed = true;

    const auto northPath = directory / "north-utm29.net.xml";

    Check(
        WriteFile(
            northPath,
            NetWithLocation(
                "netOffset=\"-526913.640,-4576484.190\" "
                "projParameter=\"+proj=utm +zone=29 "
                "+ellps=WGS84 +datum=WGS84 +units=m +no_defs\"")),
        "WRITE_NORTH_UTM_FIXTURE",
        passed);

    ns3::SumoUtmWgs84ProjectionConfig north{};
    std::string error = "stale";

    const bool northLoaded =
        ns3::SumoNetworkProjectionLoader::LoadUtmWgs84(
            northPath.string(), north, error);

    Check(
        northLoaded,
        "NORTH_UTM_WGS84_CONFIGURATION_LOADS",
        passed);

    Check(
        north.zone == 29 &&
        north.northernHemisphere &&
        north.netOffsetXMeters == -526913.640 &&
        north.netOffsetYMeters == -4576484.190,
        "NORTH_UTM_ZONE_AND_OFFSET_PARSED",
        passed);

    Check(
        error.empty(),
        "SUCCESS_CLEARS_ERROR",
        passed);

    if (northLoaded)
    {
        const ns3::SumoProjectedPosition projected{
            20374.270, 9640.360
        };

        ns3::SumoWgs84Position geographic{};
        error = "stale";

        const bool converted =
            ns3::SumoGeodeticTransform::ToWgs84(
                north, projected, geographic, error);

        Check(
            converted &&
            std::abs(geographic.latitudeDeg - 41.425290000) <= 1e-7 &&
            std::abs(geographic.longitudeDeg - (-8.434076200)) <= 1e-7,
            "LOADED_CONFIGURATION_CONVERTS_REFERENCE_POINT",
            passed);
    }

    const auto southPath = directory / "south-utm.net.xml";

    Check(
        WriteFile(
            southPath,
            NetWithLocation(
                "netOffset=\"100.0,200.0\" "
                "projParameter=\"+proj=utm +zone=29 +south "
                "+ellps=WGS84 +datum=WGS84 +units=m +no_defs\"")),
        "WRITE_SOUTH_UTM_FIXTURE",
        passed);

    ns3::SumoUtmWgs84ProjectionConfig south{};
    error = "stale";

    const bool southLoaded =
        ns3::SumoNetworkProjectionLoader::LoadUtmWgs84(
            southPath.string(), south, error);

    Check(
        southLoaded &&
        south.zone == 29 &&
        !south.northernHemisphere &&
        south.netOffsetXMeters == 100.0 &&
        south.netOffsetYMeters == 200.0,
        "SOUTHERN_HEMISPHERE_PARSED",
        passed);

    const auto missingLocation = directory / "missing-location.net.xml";
    WriteFile(missingLocation, "<net></net>");

    ExpectRejected(
        missingLocation,
        "MISSING_LOCATION_REJECTED",
        passed);

    const auto missingOffset = directory / "missing-offset.net.xml";
    WriteFile(
        missingOffset,
        NetWithLocation(
            "projParameter=\"+proj=utm +zone=29 "
            "+datum=WGS84 +units=m\""));

    ExpectRejected(
        missingOffset,
        "MISSING_NET_OFFSET_REJECTED",
        passed);

    const auto malformedOffset = directory / "malformed-offset.net.xml";
    WriteFile(
        malformedOffset,
        NetWithLocation(
            "netOffset=\"10.0\" "
            "projParameter=\"+proj=utm +zone=29 "
            "+datum=WGS84 +units=m\""));

    ExpectRejected(
        malformedOffset,
        "MALFORMED_NET_OFFSET_REJECTED",
        passed);

    const auto nonFiniteOffset = directory / "non-finite-offset.net.xml";
    WriteFile(
        nonFiniteOffset,
        NetWithLocation(
            "netOffset=\"nan,0\" "
            "projParameter=\"+proj=utm +zone=29 "
            "+datum=WGS84 +units=m\""));

    ExpectRejected(
        nonFiniteOffset,
        "NON_FINITE_NET_OFFSET_REJECTED",
        passed);

    const auto noProjection = directory / "no-projection.net.xml";
    WriteFile(
        noProjection,
        NetWithLocation("netOffset=\"0,0\""));

    ExpectRejected(
        noProjection,
        "MISSING_PROJECTION_REJECTED",
        passed);

    const auto localProjection = directory / "local-projection.net.xml";
    WriteFile(
        localProjection,
        NetWithLocation(
            "netOffset=\"0,0\" projParameter=\"!\""));

    ExpectRejected(
        localProjection,
        "UNREFERENCED_LOCAL_NETWORK_REJECTED",
        passed);

    const auto wrongProjection = directory / "wrong-projection.net.xml";
    WriteFile(
        wrongProjection,
        NetWithLocation(
            "netOffset=\"0,0\" "
            "projParameter=\"+proj=longlat +datum=WGS84\""));

    ExpectRejected(
        wrongProjection,
        "NON_UTM_PROJECTION_REJECTED",
        passed);

    const auto wrongDatum = directory / "wrong-datum.net.xml";
    WriteFile(
        wrongDatum,
        NetWithLocation(
            "netOffset=\"0,0\" "
            "projParameter=\"+proj=utm +zone=29 "
            "+datum=ETRS89 +units=m\""));

    ExpectRejected(
        wrongDatum,
        "NON_WGS84_DATUM_REJECTED",
        passed);

    const auto missingDatum = directory / "missing-datum.net.xml";
    WriteFile(
        missingDatum,
        NetWithLocation(
            "netOffset=\"0,0\" "
            "projParameter=\"+proj=utm +zone=29 +units=m\""));

    ExpectRejected(
        missingDatum,
        "MISSING_WGS84_DATUM_REJECTED",
        passed);

    const auto wrongUnits = directory / "wrong-units.net.xml";
    WriteFile(
        wrongUnits,
        NetWithLocation(
            "netOffset=\"0,0\" "
            "projParameter=\"+proj=utm +zone=29 "
            "+datum=WGS84 +units=ft\""));

    ExpectRejected(
        wrongUnits,
        "NON_METRIC_PROJECTION_REJECTED",
        passed);

    const auto invalidZone = directory / "invalid-zone.net.xml";
    WriteFile(
        invalidZone,
        NetWithLocation(
            "netOffset=\"0,0\" "
            "projParameter=\"+proj=utm +zone=61 "
            "+datum=WGS84 +units=m\""));

    ExpectRejected(
        invalidZone,
        "INVALID_UTM_ZONE_REJECTED",
        passed);

    const auto missingZone = directory / "missing-zone.net.xml";
    WriteFile(
        missingZone,
        NetWithLocation(
            "netOffset=\"0,0\" "
            "projParameter=\"+proj=utm +datum=WGS84 +units=m\""));

    ExpectRejected(
        missingZone,
        "MISSING_UTM_ZONE_REJECTED",
        passed);

    const auto duplicateZone = directory / "duplicate-zone.net.xml";
    WriteFile(
        duplicateZone,
        NetWithLocation(
            "netOffset=\"0,0\" "
            "projParameter=\"+proj=utm +zone=29 +zone=30 "
            "+datum=WGS84 +units=m\""));

    ExpectRejected(
        duplicateZone,
        "DUPLICATE_UTM_ZONE_REJECTED",
        passed);

    const auto malformedXml = directory / "malformed.net.xml";
    WriteFile(malformedXml, "<net><location");

    ExpectRejected(
        malformedXml,
        "MALFORMED_XML_REJECTED",
        passed);

    const auto duplicateLocation = directory / "duplicate-location.net.xml";
    WriteFile(
        duplicateLocation,
        "<net>"
        "<location netOffset=\"0,0\" "
        "projParameter=\"+proj=utm +zone=29 +datum=WGS84 +units=m\"/>"
        "<location netOffset=\"0,0\" "
        "projParameter=\"+proj=utm +zone=29 +datum=WGS84 +units=m\"/>"
        "</net>");

    ExpectRejected(
        duplicateLocation,
        "DUPLICATE_LOCATION_REJECTED",
        passed);

    ExpectRejected(
        directory / "does-not-exist.net.xml",
        "MISSING_NETWORK_FILE_REJECTED",
        passed);

    std::cout << "SUMO_NETWORK_PROJECTION_LOADER="
              << (passed ? "PASS" : "FAIL") << '\n';

    return passed ? 0 : 1;
}
