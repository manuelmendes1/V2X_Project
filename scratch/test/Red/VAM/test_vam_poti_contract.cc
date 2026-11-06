#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

#include "vanetza_integration/vam/poti/vam_poti_sample.h"
#include "vanetza_integration/vam/poti/vam_poti_validation.h"
#include "vanetza_integration/vam/poti/vam_poti_temporal_transform.h"

namespace
{

int failures = 0;

void Check(bool condition, const std::string& description)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << description << '\n';
        ++failures;
    }
    else
    {
        std::cout << "[PASS] " << description << '\n';
    }
}

VamPotiSample ValidSample()
{
    VamPotiSample sample;

    sample.latitudeDeg = 41.4075;
    sample.longitudeDeg = -8.5198;
    sample.altitudeMeters = std::nullopt;

    sample.speedMps = 1.4;
    sample.headingDeg = 90.0;
    sample.longitudinalAccelerationMps2 = 0.2;

    sample.positionTimeSimulationUs = 200000;
    sample.generationTimeSimulationUs = 1200000;

    return sample;
}

void TestRequiredFields()
{
    std::string error;

    VamPotiSample empty;
    Check(
        !VamPotiValidation::Validate(empty, error),
        "completely empty sample is rejected");

    auto sample = ValidSample();
    sample.latitudeDeg.reset();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "missing latitude is rejected");

    sample = ValidSample();
    sample.longitudeDeg.reset();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "missing longitude is rejected");

    sample = ValidSample();
    sample.speedMps.reset();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "missing speed is rejected");

    sample = ValidSample();
    sample.headingDeg.reset();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "missing heading is rejected");

    sample = ValidSample();
    sample.positionTimeSimulationUs.reset();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "missing position timestamp is rejected");

    sample = ValidSample();
    sample.generationTimeSimulationUs.reset();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "missing generation timestamp is rejected");
}

void TestValidGeodeticPosition()
{
    auto sample = ValidSample();
    std::string error;

    Check(
        VamPotiValidation::Validate(sample, error),
        "valid WGS84 position and kinematics are accepted");

    sample.latitudeDeg = 90.0;
    Check(
        VamPotiValidation::Validate(sample, error),
        "latitude upper boundary is accepted");

    sample.latitudeDeg = -90.0;
    Check(
        VamPotiValidation::Validate(sample, error),
        "latitude lower boundary is accepted");

    sample = ValidSample();
    sample.longitudeDeg = 180.0;
    Check(
        VamPotiValidation::Validate(sample, error),
        "longitude upper boundary is accepted");

    sample.longitudeDeg = -180.0;
    Check(
        VamPotiValidation::Validate(sample, error),
        "longitude lower boundary is accepted");
}

void TestInvalidGeodeticPosition()
{
    auto sample = ValidSample();
    std::string error;

    sample.latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "NaN latitude is rejected");

    sample = ValidSample();
    sample.latitudeDeg = std::numeric_limits<double>::infinity();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "infinite latitude is rejected");

    sample = ValidSample();
    sample.latitudeDeg = 90.000001;
    Check(
        !VamPotiValidation::Validate(sample, error),
        "latitude above WGS84 range is rejected");

    sample = ValidSample();
    sample.longitudeDeg = -180.000001;
    Check(
        !VamPotiValidation::Validate(sample, error),
        "longitude below WGS84 range is rejected");

    sample = ValidSample();
    sample.longitudeDeg = std::numeric_limits<double>::quiet_NaN();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "NaN longitude is rejected");
}

void TestOptionalMeasurements()
{
    auto sample = ValidSample();
    std::string error;

    sample.altitudeMeters = std::nullopt;
    sample.longitudinalAccelerationMps2 = std::nullopt;

    Check(
        VamPotiValidation::Validate(sample, error),
        "unavailable altitude and acceleration remain optional");

    sample = ValidSample();
    sample.altitudeMeters =
        std::numeric_limits<double>::quiet_NaN();

    Check(
        !VamPotiValidation::Validate(sample, error),
        "non-finite altitude is rejected when present");

    sample = ValidSample();
    sample.longitudinalAccelerationMps2 =
        std::numeric_limits<double>::infinity();

    Check(
        !VamPotiValidation::Validate(sample, error),
        "non-finite acceleration is rejected when present");
}

void TestKinematics()
{
    auto sample = ValidSample();
    std::string error;

    sample.speedMps = std::numeric_limits<double>::quiet_NaN();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "non-finite speed is rejected");

    sample = ValidSample();
    sample.speedMps = -0.01;
    Check(
        !VamPotiValidation::Validate(sample, error),
        "negative speed is rejected");

    sample = ValidSample();
    sample.headingDeg = std::numeric_limits<double>::infinity();
    Check(
        !VamPotiValidation::Validate(sample, error),
        "non-finite heading is rejected");

    sample = ValidSample();
    sample.headingDeg = -0.01;
    Check(
        !VamPotiValidation::Validate(sample, error),
        "negative SUMO heading is rejected");

    sample = ValidSample();
    sample.headingDeg = 360.001;
    Check(
        !VamPotiValidation::Validate(sample, error),
        "SUMO heading above 360 degrees is rejected");

    const double cardinalHeadings[] = {
        0.0, 90.0, 180.0, 270.0, 360.0
    };

    for (const double heading : cardinalHeadings)
    {
        sample = ValidSample();
        sample.headingDeg = heading;

        Check(
            VamPotiValidation::Validate(sample, error),
            "cardinal SUMO heading " + std::to_string(heading)
                + " degrees is accepted");
    }
}

void TestTemporalReference()
{
    auto sample = ValidSample();
    std::string error;

    VamPotiTimeReference output{};

    constexpr std::int64_t itsStartClockUs = 10000000;

    Check(
        VamPotiTemporalTransform::BuildReference(
            itsStartClockUs, sample, output, error),
        "valid SUMO sample maps to the ITS clock domain");

    Check(
        output.positionTimestampItsUs == 10200000,
        "position timestamp preserves the sample acquisition instant");

    Check(
        output.generationTimestampItsUs == 11200000,
        "generation timestamp uses the SUMO generation instant");

    Check(
        output.positionAgeUs == 1000000,
        "position age is derived from generation minus acquisition");
}

void TestZeroTemporalReference()
{
    auto sample = ValidSample();
    sample.positionTimeSimulationUs = 0;
    sample.generationTimeSimulationUs = 0;

    std::string error;
    VamPotiTimeReference reference{
        11,
        22,
        33
    };

    const bool built =
        VamPotiTemporalTransform::BuildReference(
            0,
            sample,
            reference,
            error);

    Check(
        built,
        "zero simulation timestamps and zero ITS start are accepted");

    if (!built)
    {
        return;
    }

    Check(
        reference.positionTimestampItsUs == 0,
        "zero position simulation time maps to ITS timestamp zero");

    Check(
        reference.generationTimestampItsUs == 0,
        "zero generation simulation time maps to ITS timestamp zero");

    Check(
        reference.positionAgeUs == 0,
        "identical zero timestamps produce zero position age");

    std::uint16_t generationDeltaTime = 123;

    Check(
        VamPotiTemporalTransform::ComputeGenerationDeltaTime(
            reference.positionTimestampItsUs,
            generationDeltaTime,
            error),
        "GDT accepts the zero ITS position reference timestamp");

    Check(
        generationDeltaTime == 0,
        "zero ITS position reference produces GDT zero");
}

void TestTemporalAlignmentBoundary()
{
    auto sample = ValidSample();
    std::string error;
    VamPotiTimeReference output{};

    sample.positionTimeSimulationUs = 0;
    sample.generationTimeSimulationUs = 32766999;

    Check(
        VamPotiTemporalTransform::BuildReference(
            1000000, sample, output, error),
        "position age of 32766.999 ms is accepted");

    sample.generationTimeSimulationUs = 32767000;

    Check(
        !VamPotiTemporalTransform::BuildReference(
            1000000, sample, output, error),
        "position age of exactly 32767 ms is rejected");
}

void TestInvalidTemporalSamples()
{
    auto sample = ValidSample();
    std::string error;
    VamPotiTimeReference output{};

    sample.generationTimeSimulationUs =
        *sample.positionTimeSimulationUs - 1;

    Check(
        !VamPotiTemporalTransform::BuildReference(
            1000000, sample, output, error),
        "generation before position acquisition is rejected");

    sample = ValidSample();
    sample.positionTimeSimulationUs =
        std::numeric_limits<std::uint64_t>::max();

    Check(
        !VamPotiTemporalTransform::BuildReference(
            1000000, sample, output, error),
        "simulation timestamp outside int64 range is rejected");

    sample = ValidSample();

    Check(
        !VamPotiTemporalTransform::BuildReference(
            -1, sample, output, error),
        "negative ITS start time is rejected");
}

void TestGenerationDeltaTime()
{
    std::string error;
    std::uint16_t output = 123;

    Check(
        VamPotiTemporalTransform::ComputeGenerationDeltaTime(
            65535999, output, error),
        "GDT accepts the last millisecond before rollover");

    Check(
        output == 65535,
        "GDT truncates sub-millisecond precision before rollover");

    Check(
        VamPotiTemporalTransform::ComputeGenerationDeltaTime(
            65536000, output, error),
        "GDT accepts the exact 16-bit rollover boundary");

    Check(
        output == 0,
        "GDT wraps to zero at 65536 milliseconds");

    output = 123;

    Check(
        !VamPotiTemporalTransform::ComputeGenerationDeltaTime(
            -1, output, error),
        "GDT rejects a negative ITS position timestamp");

    Check(
        output == 123,
        "failed GDT conversion leaves output unchanged");
}

void TestGenerationDeltaTimeUsesPositionReference()
{
    auto sample = ValidSample();
    std::string error;

    VamPotiTimeReference reference{};
    std::uint16_t generationDeltaTime = 0;

    const bool referenceBuilt =
        VamPotiTemporalTransform::BuildReference(
            10000000,
            sample,
            reference,
            error);

    Check(
        referenceBuilt,
        "temporal reference is built before GDT conversion");

    if (!referenceBuilt)
    {
        return;
    }

    Check(
        VamPotiTemporalTransform::ComputeGenerationDeltaTime(
            reference.positionTimestampItsUs,
            generationDeltaTime,
            error),
        "GDT is computed from the position reference timestamp");

    Check(
        generationDeltaTime == 10200,
        "GDT represents position time, not generation time");

    Check(
        generationDeltaTime !=
            static_cast<std::uint16_t>(
                (reference.generationTimestampItsUs / 1000) % 65536),
        "GDT does not use the message generation timestamp");
}

void TestTemporalOverflowAndAtomicOutput()
{
    auto sample = ValidSample();
    std::string error;

    VamPotiTimeReference output{
        11,
        22,
        33
    };

    const auto original = output;

    const auto maxInt64 =
        std::numeric_limits<std::int64_t>::max();

    sample.positionTimeSimulationUs = 0;
    sample.generationTimeSimulationUs = 1000;

    Check(
        !VamPotiTemporalTransform::BuildReference(
            maxInt64 - 500, sample, output, error),
        "ITS clock addition overflow is rejected");

    Check(
        output.positionTimestampItsUs ==
                original.positionTimestampItsUs &&
        output.generationTimestampItsUs ==
                original.generationTimestampItsUs &&
        output.positionAgeUs == original.positionAgeUs,
        "failed temporal conversion leaves output unchanged");
}

} // namespace

int main()
{
    TestRequiredFields();
    TestValidGeodeticPosition();
    TestInvalidGeodeticPosition();
    TestOptionalMeasurements();
    TestKinematics();
    TestTemporalReference();
    TestZeroTemporalReference();
    TestTemporalAlignmentBoundary();
    TestInvalidTemporalSamples();
    TestGenerationDeltaTime();
    TestGenerationDeltaTimeUsesPositionReference();
    TestTemporalOverflowAndAtomicOutput();

    if (failures != 0)
    {
        std::cerr << "VAM_POTI_CONTRACT_FAILURES=" << failures << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "VAM_POTI_CONTRACT=PASS\n";
    return EXIT_SUCCESS;
}
