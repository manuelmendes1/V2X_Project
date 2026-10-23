#ifndef VANETZA_INTEGRATION_CPM_CODEC_CPM_UPER_DECODER_H
#define VANETZA_INTEGRATION_CPM_CODEC_CPM_UPER_DECODER_H

#include "vanetza_integration/cpm/types/cpm_message.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct CpmUperDecodeResult
{
    bool success = false;
    std::optional<CpmMessage> message;
    std::string error;
};

class CpmUperDecoder
{
public:
    // Descoding of the supported CPM R2 RSU profile.
    // receivedAtUnixMs is local reception metadata and is not read from
    // the encoded CPM. Unsupported fields are rejected, not discarded.
    static CpmUperDecodeResult Decode(
        const std::vector<std::uint8_t>& payload,
        std::int64_t receivedAtUnixMs);
};

#endif
