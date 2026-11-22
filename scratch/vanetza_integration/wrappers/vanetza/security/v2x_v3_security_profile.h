#pragma once

#include <cstdint>

namespace ns3
{

enum class V2xV3SecurityProfile : std::uint8_t
{
    Unsupported = 0,
    VehicleCamDenm,
    SpatemRsu,
    CpmRsu
};

constexpr V2xV3SecurityProfile
ClassifyV2xV3SecurityProfile(std::uint32_t aid) noexcept
{
    switch (aid)
    {
    case 36:
    case 37:
        return V2xV3SecurityProfile::VehicleCamDenm;
    case 137:
        return V2xV3SecurityProfile::SpatemRsu;
    case 639:
        return V2xV3SecurityProfile::CpmRsu;
    default:
        return V2xV3SecurityProfile::Unsupported;
    }
}

} // namespace ns3
