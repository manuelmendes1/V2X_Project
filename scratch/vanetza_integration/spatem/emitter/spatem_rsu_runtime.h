#pragma once

#include <memory>
#include <string>

#include "vanetza_integration/context/packet/packet_context.h"
#include "vanetza_integration/spatem/types/spatem_input.h"

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

class SpatemEmitterEndpoint;
class SpatemRsuPositionProvider;
class Ns3AccessAdapter;
class Ns3DccAdapter;
class VanetzaBtpAdapter;

class SpatemRsuRuntime
{
public:
    explicit SpatemRsuRuntime(SpatemEmitterEndpoint& endpoint);
    ~SpatemRsuRuntime();

    SpatemRsuRuntime(const SpatemRsuRuntime&) = delete;
    SpatemRsuRuntime& operator=(const SpatemRsuRuntime&) = delete;

    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    bool Send(
        const std::shared_ptr<PacketContext>& context,
        const SpatemInput& input,
        const std::string& reason = {});

private:
    SpatemEmitterEndpoint& m_endpoint;

    std::unique_ptr<vanetza::ManualRuntime> m_runtime;
    std::unique_ptr<vanetza::security::SecurityEntity> m_securityEntity;
    std::unique_ptr<vanetza::geonet::MIB> m_mib;
    std::unique_ptr<vanetza::geonet::Router> m_router;

    std::unique_ptr<SpatemRsuPositionProvider> m_position;
    std::unique_ptr<Ns3AccessAdapter> m_access;
    std::unique_ptr<Ns3DccAdapter> m_dccAdapter;
    std::unique_ptr<VanetzaBtpAdapter> m_btpAdapter;

    bool m_initialized = false;
};

} // namespace ns3
