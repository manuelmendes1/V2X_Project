#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/wrappers/vanetza/spatem/vanetza_spatem_codec.h"

namespace
{

bool CheckDecodeFailure(
    const std::string& name,
    const std::vector<uint8_t>& payload,
    const std::string& expectedError)
{
    const auto result = VanetzaSpatemCodec::Decode(payload);

    const bool passed =
        !result.success &&
        result.error == expectedError;

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << name
              << " success=" << result.success
              << " error=\"" << result.error << "\"\n";

    return passed;
}

} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    if (CheckDecodeFailure(
            "codec_empty_payload_rejected",
            {},
            "empty payload"))
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    if (CheckDecodeFailure(
            "codec_truncated_uper_rejected",
            {0x02},
            "ASN.1 decode failed"))
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    std::cout << "SPATEM_CODEC_MALFORMED_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_CODEC_MALFORMED_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_CODEC_MALFORMED_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_CODEC_MALFORMED_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
