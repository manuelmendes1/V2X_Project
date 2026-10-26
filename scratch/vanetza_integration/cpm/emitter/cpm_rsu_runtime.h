#pragma once

#include <memory>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/cpm/types/cpm_input.h"

namespace vanetza
{
class ManualRuntime;

namespace geonet
{
struct ManagementInformationBase;
using MIB = ManagementInformationBase;
class Router;
}

namespace security
{
class SecurityEntity;
}
}

namespace ns3
{

class CpmEmitterEndpoint;
class CpmRsuPositionProvider;
class Ns3AccessAdapter;
class Ns3DccAdapter;
class VanetzaBtpAdapter;

class CpmRsuRuntime
{
public:
    explicit CpmRsuRuntime(CpmEmitterEndpoint& endpoint);
    ~CpmRsuRuntime();

    CpmRsuRuntime(const CpmRsuRuntime&) = delete;
    CpmRsuRuntime& operator=(const CpmRsuRuntime&) = delete;

    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    bool Send(
        const std::shared_ptr<PacketContext>& context,
        const CpmInput& input,
        uint8_t trafficClass);

private:
    CpmEmitterEndpoint& m_endpoint;

    std::unique_ptr<vanetza::ManualRuntime> m_runtime;
    std::unique_ptr<vanetza::security::SecurityEntity> m_securityEntity;
    std::unique_ptr<vanetza::geonet::MIB> m_mib;
    std::unique_ptr<vanetza::geonet::Router> m_router;
    std::unique_ptr<CpmRsuPositionProvider> m_position;
    std::unique_ptr<Ns3AccessAdapter> m_access;
    std::unique_ptr<Ns3DccAdapter> m_dccAdapter;
    std::unique_ptr<VanetzaBtpAdapter> m_btpAdapter;

    bool m_initialized = false;
};

} // namespace ns3
