#include <iostream>
#include <string>

#include "MessageId.h"
#include "vanetza_integration/vam/codec/vam_header_validator.h"

namespace
{

struct HeaderCase
{
    const char* name;
    long protocolVersion;
    long messageId;
    bool expectedSuccess;
    const char* expectedError;
};

bool Check(const HeaderCase& test)
{
    Vanetza_ITS2_ItsPduHeaderVam_t header{};

    header.protocolVersion = test.protocolVersion;
    header.messageId = test.messageId;
    header.stationId = 9001;

    const auto result = VamHeaderValidator::Validate(header);

    const std::string expectedError = test.expectedError;
    const bool passed =
        result.success == test.expectedSuccess &&
        result.error == expectedError;

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << test.name
              << " success=" << result.success
              << " error=\"" << result.error << "\"\n";

    return passed;
}

} // namespace

int main()
{
    static_assert(Vanetza_ITS2_MessageId_vam == 16);

    const HeaderCase cases[] = {
        {
            "VALID_VAM_HEADER",
            3,
            Vanetza_ITS2_MessageId_vam,
            true,
            ""
        },
        {
            "WRONG_PROTOCOL_VERSION",
            2,
            Vanetza_ITS2_MessageId_vam,
            false,
            "unsupported VAM protocolVersion"
        },
        {
            "WRONG_MESSAGE_ID",
            3,
            Vanetza_ITS2_MessageId_cam,
            false,
            "invalid VAM messageID"
        },
        {
            "BOTH_FIELDS_INVALID",
            2,
            Vanetza_ITS2_MessageId_cam,
            false,
            "unsupported VAM protocolVersion"
        }
    };

    unsigned passed = 0;
    unsigned failed = 0;

    for (const auto& test : cases)
    {
        if (Check(test))
        {
            ++passed;
        }
        else
        {
            ++failed;
        }
    }

    std::cout << "VAM_HEADER_VALIDATION_CASES="
              << passed + failed << '\n';
    std::cout << "VAM_HEADER_VALIDATION_PASSED="
              << passed << '\n';
    std::cout << "VAM_HEADER_VALIDATION_FAILED="
              << failed << '\n';
    std::cout << "VAM_HEADER_VALIDATION_TESTS="
              << (failed == 0 ? "PASS" : "FAIL") << '\n';

    return failed == 0 ? 0 : 1;
}
