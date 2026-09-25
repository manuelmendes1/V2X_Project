#pragma once

#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"

namespace ns3
{

// ============================================================================
// ActiveCredentialProvider
// ----------------------------------------------------------------------------
// Read-only security boundary for consumers that need the credential
// currently selected for a vehicle.
//
// The provider exposes only the active credential. It does not expose:
//   - pool indexing
//   - rotation operations
//   - discovery
//   - filesystem access
//
// The concrete owner of the credential remains responsible for selection.
// ============================================================================
class ActiveCredentialProvider
{
public:
    virtual ~ActiveCredentialProvider() = default;

    virtual const VehicleCredential* GetActiveCredential() const = 0;
};

} // namespace ns3
