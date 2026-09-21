#include "vehicle_security_initializer.h"

#include "vanetza_integration/security/vehicle_crypto/vehicle_crypto.h"

#include <cstdlib>
#include <string>

namespace ns3 {

    void VehicleSecurityInitializer::Initialize(VehicleState& state){
        // ----------------------------------------------------------------------
        // Legacy/local OpenSSL material.
        //
        // Mantido para não alterar o mecanismo CAM existente nesta fase.
        // ----------------------------------------------------------------------
        auto [key, cert] = GenerateEcdsaKeyAndCert(state.identity.vehId);

        state.security.privateKey = key;
        state.security.certificate = cert;

        // ----------------------------------------------------------------------
        // Real ETSI ITS Security V3 credential paths for DENM.
        //
        // Activated only when V2X_DENM_PKI_DIR is defined.
        //
        // Expected layout:
        //
        //   <dir>/veh0-ticket.cert
        //   <dir>/veh0-ticket.key
        //   <dir>/veh1-ticket.cert
        //   <dir>/veh1-ticket.key
        //   ...
        // ----------------------------------------------------------------------
        const char* pkiDir = std::getenv("V2X_DENM_PKI_DIR");

        if (pkiDir != nullptr && *pkiDir != '\0'){

            const std::string baseDir = pkiDir;

            state.security.certFile = baseDir + "/" + state.identity.vehId + "-ticket.cert";

            state.security.keyFile = baseDir + "/" + state.identity.vehId + "-ticket.pem";
        }
    }

} // namespace ns3