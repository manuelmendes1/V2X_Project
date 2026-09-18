#include "packet_payload.h"

#include <iomanip>
#include <sstream>

namespace ns3
{

bool PacketPayload::Empty() const
{
    return bytes.empty();
}

std::size_t PacketPayload::Size() const
{
    return bytes.size();
}

void PacketPayload::Clear()
{
    type = PayloadType::NONE;

    bytes.clear();

    signatureSize = 0;
}

std::string PacketPayload::ToHex(std::size_t maxBytes) const
{
    if (bytes.empty())
    {
        return "";
    }

    std::ostringstream oss;

    oss << std::uppercase
        << std::hex
        << std::setfill('0');

    const std::size_t limit =
        std::min(maxBytes, bytes.size());

    for (std::size_t i = 0; i < limit; ++i)
    {
        oss << std::setw(2)
            << static_cast<unsigned>(bytes[i]);
    }

    if (limit < bytes.size())
    {
        oss << "...";
    }

    return oss.str();
}

} // namespace ns3
