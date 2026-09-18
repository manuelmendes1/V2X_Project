#include "vehicle_security_initializer.h"

#include "vanetza_integration/security/vehicle_crypto/vehicle_crypto.h"

namespace ns3
{

void VehicleSecurityInitializer::Initialize(VehicleState& state){

    auto [key, cert] = GenerateEcdsaKeyAndCert(state.identity.vehId);

    state.security.privateKey = key;
    state.security.certificate = cert;
}

} // namespace ns3
