#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/cam/types/cam_input.h"
#include "vanetza_integration/wrappers/vanetza/cam/vanetza_cam_codec.h"

namespace
{

struct PositionCase
{
    const char* name;
    double latitude;
    double longitude;
};

bool CheckRoundTrip(const PositionCase& testCase)
{
    CamInput input{};
    input.stationId = 4242;
    input.latitude = testCase.latitude;
    input.longitude = testCase.longitude;
    input.speed = 13.5;
    input.heading = 90.0;
    input.acceleration = 0.0;
    input.generationDeltaTime = 1234;

    const auto encoded = VanetzaCamCodec::Encode(input);

    if (!encoded.success || encoded.bytes.empty())
    {
        std::cerr
            << "[FAIL] " << testCase.name
            << " encode_success=" << encoded.success
            << " bytes=" << encoded.bytes.size()
            << " failed_type=" << encoded.failedType
            << '\n';
        return false;
    }

    const auto decoded =
        VanetzaCamCodec::Decode(encoded.bytes, 777);

    if (!decoded.success)
    {
        std::cerr
            << "[FAIL] " << testCase.name
            << " decode_error=\"" << decoded.error << "\"\n";
        return false;
    }

    const auto& actual = decoded.message;
    constexpr double tolerance = 1e-5;

    if (actual.stationId != input.stationId ||
        std::abs(actual.latitude - input.latitude) > tolerance ||
        std::abs(actual.longitude - input.longitude) > tolerance ||
        actual.generationDeltaTime != input.generationDeltaTime)
    {
        std::cerr
            << "[FAIL] " << testCase.name
            << " expected=(" << input.latitude << ','
            << input.longitude << ')'
            << " actual=(" << actual.latitude << ','
            << actual.longitude << ')'
            << " expected_station=" << input.stationId
            << " actual_station=" << actual.stationId
            << " expected_gdt=" << input.generationDeltaTime
            << " actual_gdt=" << actual.generationDeltaTime
            << '\n';
        return false;
    }

    std::cout
        << "[PASS] " << testCase.name
        << " latitude=" << actual.latitude
        << " longitude=" << actual.longitude
        << " bytes=" << encoded.bytes.size()
        << '\n';

    return true;
}

} // namespace

int main()
{
    const std::array<PositionCase, 3> cases{{
        {"interior_position", 500.0, 500.0},
        {"veh2_observed_position", 995.05, 522.507},
        {"scenario_upper_boundary", 1000.0, 1000.0},
    }};

    unsigned passed = 0;
    unsigned failed = 0;

    for (const auto& testCase : cases)
    {
        if (CheckRoundTrip(testCase))
        {
            ++passed;
        }
        else
        {
            ++failed;
        }
    }

    std::cout << "CAM_COORDINATE_BOUNDS_CASES=" << cases.size() << '\n';
    std::cout << "CAM_COORDINATE_BOUNDS_PASSED=" << passed << '\n';
    std::cout << "CAM_COORDINATE_BOUNDS_FAILED=" << failed << '\n';
    std::cout << "CAM_COORDINATE_BOUNDS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
