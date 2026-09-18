
#include "vehicle_registry.h"

#include "ns3/log.h"

#include <iomanip>
#include <sstream>

namespace ns3
{

namespace
{

std::string
GeoNetMidToString(const GeoNetMid& mid)
{
    std::ostringstream oss;

    for (std::size_t i = 0; i < mid.size(); ++i)
    {
        if (i != 0)
        {
            oss << ":";
        }

        oss << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(mid[i]);
    }

    return oss.str();
}

} // namespace

VehicleRegistry&
VehicleRegistry::Instance()
{
    static VehicleRegistry instance;
    return instance;
}

VehicleState*
VehicleRegistry::CreateOrGet(
    int64_t step,
    const std::string& vehId)
{
    (void)step;

    auto it = vehiclesById.find(vehId);

    if (it != vehiclesById.end()) {
        return it->second.get();
    }

    auto vs = std::make_unique<VehicleState>();

    /*
     * VehicleRegistry é responsável apenas pela gestão e indexação
     * dos VehicleState.
     *
     * A autoridade sobre a identidade pertence ao
     * VehicleIdentityInitializer.
     *
     * Neste ponto apenas o vehId é conhecido, pois é o identificador
     * proveniente do SUMO / VehicleBuilder.
     */
    vs->identity.vehId = vehId;
    vs->initialized = false;

    VehicleState* ptr = vs.get();

    vehiclesById[vehId] = std::move(vs);

    return ptr;
}

VehicleState*
VehicleRegistry::GetByVehId(
    const std::string& vehId)
{
    auto it = vehiclesById.find(vehId);

    if (it == vehiclesById.end()) {
        return nullptr;
    }

    return it->second.get();
}

VehicleState*
VehicleRegistry::GetByNodeId(
    uint32_t nodeId)
{
    auto it = nodeToVeh.find(nodeId);

    if (it == nodeToVeh.end()) {
        return nullptr;
    }

    return GetByVehId(it->second);
}

VehicleState*
VehicleRegistry::GetByStationId(
    uint32_t stationId)
{
    auto it = stationToVeh.find(stationId);

    if (it == stationToVeh.end()) {
        return nullptr;
    }

    return GetByVehId(it->second);
}

VehicleState*
VehicleRegistry::GetByLinkMac(
    const Mac48Address& mac)
{
    auto it = m_byLinkMac.find(mac);

    if (it == m_byLinkMac.end())
    {
        return nullptr;
    }

    return it->second;
}

VehicleState*
VehicleRegistry::GetByIpv4(
    const Ipv4Address& address)
{
    auto it = m_byIpv4.find(address);

    if (it == m_byIpv4.end()) {
        return nullptr;
    }

    return it->second;
}

void
VehicleRegistry::RegisterNodeId(
    int64_t step,
    const std::string& vehId,
    uint32_t nodeId)
{
    (void)step;

    VehicleState* vs = GetByVehId(vehId);

    NS_ABORT_MSG_IF(
        vs == nullptr,
        "[REGISTRY] RegisterNodeId: vehicle not found: "
            << vehId);

    /*
     * VehicleIdentityInitializer é a autoridade sobre nodeId.
     * O Registry apenas verifica a consistência e cria o índice reverso.
     */
    NS_ABORT_MSG_IF(
        vs->identity.nodeId != nodeId,
        "[REGISTRY] nodeId mismatch for vehicle "
            << vehId
            << ": identity.nodeId="
            << vs->identity.nodeId
            << " requested="
            << nodeId);

    auto existing = nodeToVeh.find(nodeId);

    NS_ABORT_MSG_IF(
        existing != nodeToVeh.end()
            && existing->second != vehId,
        "[REGISTRY] nodeId collision: nodeId="
            << nodeId
            << " already belongs to vehicle "
            << existing->second
            << ", cannot register for "
            << vehId);

    nodeToVeh[nodeId] = vehId;
}

void
VehicleRegistry::RegisterStationId(
    int64_t step,
    const std::string& vehId,
    uint32_t stationId)
{
    (void)step;

    VehicleState* vs = GetByVehId(vehId);

    NS_ABORT_MSG_IF(
        vs == nullptr,
        "[REGISTRY] RegisterStationId: vehicle not found: "
            << vehId);

    /*
     * VehicleIdentityInitializer é a autoridade sobre stationId.
     * O Registry apenas verifica a consistência e cria o índice reverso.
     */
    NS_ABORT_MSG_IF(
        vs->identity.stationId != stationId,
        "[REGISTRY] stationId mismatch for vehicle "
            << vehId
            << ": identity.stationId="
            << vs->identity.stationId
            << " requested="
            << stationId);

    auto existing = stationToVeh.find(stationId);

    NS_ABORT_MSG_IF(
        existing != stationToVeh.end()
            && existing->second != vehId,
        "[REGISTRY] stationId collision: stationId="
            << stationId
            << " already belongs to vehicle "
            << existing->second
            << ", cannot register for "
            << vehId);

    stationToVeh[stationId] = vehId;
}

void
VehicleRegistry::RegisterLinkMac(
    const Mac48Address& mac,
    ns3::VehicleState& vehicle)
{
    auto existing = m_byLinkMac.find(mac);

    NS_ABORT_MSG_IF(
        existing != m_byLinkMac.end()
            && existing->second != &vehicle,
        "[REGISTRY] Link MAC collision: "
            << mac
            << " already belongs to another vehicle");

    m_byLinkMac[mac] = &vehicle;
}

void
VehicleRegistry::RegisterMid(
    const GeoNetMid& mid,
    VehicleState& vehicle)
{
    auto existing = m_byMid.find(mid);

    NS_ABORT_MSG_IF(
        existing != m_byMid.end()
            && existing->second != &vehicle,
        "[REGISTRY] GN MID collision: "
            << GeoNetMidToString(mid)
            << " already belongs to another vehicle");

    m_byMid[mid] = &vehicle;
}

void
VehicleRegistry::RegisterIpv4(
    const Ipv4Address& address,
    ns3::VehicleState& vehicle)
{
    NS_ABORT_MSG_IF(
        address == Ipv4Address::GetAny(),
        "[REGISTRY] Cannot register unspecified IPv4 address");

    auto existing = m_byIpv4.find(address);

    NS_ABORT_MSG_IF(
        existing != m_byIpv4.end()
            && existing->second != &vehicle,
        "[REGISTRY] IPv4 collision: "
            << address
            << " already belongs to another vehicle");

    m_byIpv4[address] = &vehicle;
}

uint32_t
VehicleRegistry::GetStationId(
    const std::string& vehId)
{
    VehicleState* vs = GetByVehId(vehId);

    if (vs) {
        return vs->identity.stationId;
    }

    return 0;
}

void
VehicleRegistry::Clear()
{
    vehiclesById.clear();
    nodeToVeh.clear();
    stationToVeh.clear();
    m_byLinkMac.clear();
    m_byIpv4.clear();
}

const std::unordered_map<
    std::string,
    std::unique_ptr<VehicleState>>&
VehicleRegistry::GetAllVehicles() const
{
    return vehiclesById;
}

bool
VehicleRegistry::Exists(
    const std::string& vehId) const
{
    return vehiclesById.contains(vehId);
}

bool
VehicleRegistry::Empty() const
{
    return vehiclesById.empty();
}

std::size_t
VehicleRegistry::Size() const
{
    return vehiclesById.size();
}

} // namespace ns3
