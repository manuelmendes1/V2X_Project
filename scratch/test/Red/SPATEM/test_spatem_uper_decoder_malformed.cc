#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vanetza_integration/wrappers/vanetza/spatem/uper/spatem_uper_decoder.h"

namespace
{

bool CheckDecodeFailure(
    const std::string& name,
    const std::vector<uint8_t>& payload,
    const std::string& expectedError)
{
    const auto result = SpatemUperDecoder::Decode(payload);

    const bool passed =
        !result.success &&
        !result.spatem &&
        result.error == expectedError;

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << name
              << " success=" << result.success
              << " has_spatem=" << static_cast<bool>(result.spatem)
              << " error=\"" << result.error << "\"\n";

    return passed;
}

} // namespace

int main()
{
    unsigned passed = 0;
    unsigned failed = 0;

    if (CheckDecodeFailure(
            "empty_payload_rejected",
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
            "truncated_uper_payload_rejected",
            {0x02},
            "ASN.1 decode failed"))
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    std::cout << "SPATEM_MALFORMED_DECODE_CASES="
              << passed + failed << '\n';
    std::cout << "SPATEM_MALFORMED_DECODE_PASSED="
              << passed << '\n';
    std::cout << "SPATEM_MALFORMED_DECODE_FAILED="
              << failed << '\n';
    std::cout << "SPATEM_MALFORMED_DECODE_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
