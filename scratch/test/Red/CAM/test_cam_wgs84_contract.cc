#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

#include "vanetza_integration/cam/types/cam_input.h"
#include "vanetza_integration/cam/validation/cam_validator.h"
#include "vanetza_integration/wrappers/vanetza/cam/cam_asn1_builder.h"
#include "vanetza_integration/wrappers/vanetza/cam/vanetza_cam_codec.h"

namespace
{

unsigned passed = 0;
unsigned failed = 0;

void Expect(const std::string& name, bool condition)
{
    if (condition)
    {
        ++passed;
        std::cout << "[PASS] " << name << '\n';
    }
    else
    {
        ++failed;
        std::cout << "[FAIL] " << name << '\n';
    }
}

CamInput MakeInput(double latitude, double longitude)
{
    CamInput input{};
    input.stationId = 4242;
    input.latitude = latitude;
    input.longitude = longitude;
    input.speed = 13.5;
    input.heading = 90.0;
    input.acceleration = 0.0;
    input.generationDeltaTime = 1234;
    return input;
}

bool IsValid(double latitude, double longitude)
{
    return CamValidator::Validate(MakeInput(latitude, longitude));
}

void TestValidator()
{
    Expect("WGS84 Lisbon position accepted",
           IsValid(38.7223, -9.1393));

    Expect("WGS84 origin accepted",
           IsValid(0.0, 0.0));

    Expect("WGS84 north pole accepted",
           IsValid(90.0, 0.0));

    Expect("WGS84 south pole accepted",
           IsValid(-90.0, 0.0));

    Expect("WGS84 east boundary accepted",
           IsValid(0.0, 180.0));

    Expect("WGS84 west boundary accepted",
           IsValid(0.0, -180.0));

    Expect("latitude above 90 rejected",
           !IsValid(90.000001, 0.0));

    Expect("latitude below -90 rejected",
           !IsValid(-90.000001, 0.0));

    Expect("longitude above 180 rejected",
           !IsValid(0.0, 180.000001));

    Expect("longitude below -180 rejected",
           !IsValid(0.0, -180.000001));

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    Expect("NaN latitude rejected", !IsValid(nan, 10.0));
    Expect("NaN longitude rejected", !IsValid(10.0, nan));
    Expect("infinite latitude rejected", !IsValid(inf, 10.0));
    Expect("infinite longitude rejected", !IsValid(10.0, inf));
}

void TestAsn1Scale()
{
    constexpr double expectedScale = 1e7;

    const CamInput input = MakeInput(38.7223, -9.1393);
    const auto cam = CamAsn1Builder::Build(input);

    if (!cam)
    {
        Expect("ASN.1 builder allocates CAM", false);
        return;
    }

    Expect("ASN.1 latitude uses 1e7 units per degree",
           cam->cam.camParameters.basicContainer.referencePosition.latitude
               == static_cast<long>(std::llround(
                   input.latitude * expectedScale)));

    Expect("ASN.1 longitude uses 1e7 units per degree",
           cam->cam.camParameters.basicContainer.referencePosition.longitude
               == static_cast<long>(std::llround(
                   input.longitude * expectedScale)));

    const CamInput boundary = MakeInput(90.0, 180.0);
    const auto boundaryCam = CamAsn1Builder::Build(boundary);

    Expect("ASN.1 builder preserves WGS84 upper boundaries",
           boundaryCam &&
           boundaryCam->cam.camParameters.basicContainer.referencePosition.latitude
               == 900000000L &&
           boundaryCam->cam.camParameters.basicContainer.referencePosition.longitude
               == 1800000000L);
}

void TestCodecRoundTrip()
{
    struct PositionCase
    {
        const char* name;
        double latitude;
        double longitude;
    };

    const std::array<PositionCase, 4> cases{{
        {"Lisbon", 38.7223, -9.1393},
        {"Sydney", -33.8688, 151.2093},
        {"north-east boundary", 90.0, 180.0},
        {"south-west boundary", -90.0, -180.0},
    }};

    constexpr double tolerance = 5.1e-8;

    for (const auto& testCase : cases)
    {
        const CamInput input =
            MakeInput(testCase.latitude, testCase.longitude);

        const auto encoded = VanetzaCamCodec::Encode(input);

        if (!encoded.success || encoded.bytes.empty())
        {
            Expect(
                std::string("codec encodes ") + testCase.name,
                false);
            continue;
        }

        const auto decoded =
            VanetzaCamCodec::Decode(encoded.bytes, 777);

        if (!decoded.success)
        {
            Expect(
                std::string("codec decodes ") + testCase.name,
                false);
            continue;
        }

        const auto& actual = decoded.message;

        const bool positionMatches =
            std::abs(actual.latitude - input.latitude) <= tolerance &&
            std::abs(actual.longitude - input.longitude) <= tolerance;

        Expect(
            std::string("WGS84 round-trip ") + testCase.name,
            positionMatches);

        Expect(
            std::string("round-trip metadata ") + testCase.name,
            actual.stationId == input.stationId &&
            actual.generationDeltaTime == input.generationDeltaTime);
    }
}

} // namespace

int main()
{
    TestValidator();
    TestAsn1Scale();
    TestCodecRoundTrip();

    std::cout << "CAM_WGS84_CONTRACT_PASSED=" << passed << '\n';
    std::cout << "CAM_WGS84_CONTRACT_FAILED=" << failed << '\n';
    std::cout << "CAM_WGS84_CONTRACT="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
