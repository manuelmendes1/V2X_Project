#pragma once

#include "vanetza_integration/context/vehicle_security/vehicle_credential.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace ns3
{

// ============================================================================
// VehicleCredentialPool
// ----------------------------------------------------------------------------
// Owns an ordered collection of VehicleCredential descriptors and the index
// of the currently selected credential.
//
// The pool does not perform filesystem I/O and does not own loaded OpenSSL
// objects. Credential discovery and cryptographic material remain outside this
// type.
// ============================================================================
class VehicleCredentialPool
{
public:
    VehicleCredentialPool() = default;

    bool Empty() const
    {
        return m_credentials.empty();
    }

    std::size_t Size() const
    {
        return m_credentials.size();
    }

    const VehicleCredential* GetActiveCredential() const
    {
        if (m_credentials.empty() ||
            m_activeIndex >= m_credentials.size())
        {
            return nullptr;
        }

        return &m_credentials[m_activeIndex];
    }

    std::size_t GetActiveCredentialIndex() const
    {
        return m_activeIndex;
    }

    const VehicleCredential* GetCredential(
        std::size_t index) const
    {
        if (index >= m_credentials.size())
        {
            return nullptr;
        }

        return &m_credentials[index];
    }

    void SetCredentials(std::vector<VehicleCredential> credentials)
    {
        m_credentials = std::move(credentials);
        m_activeIndex = 0;
    }

    bool Select(std::size_t index)
    {
        if (index >= m_credentials.size())
        {
            return false;
        }

        m_activeIndex = index;
        return true;
    }

    bool SelectNext()
    {
        if (m_credentials.empty() ||
            m_activeIndex + 1 >= m_credentials.size())
        {
            return false;
        }

        ++m_activeIndex;
        return true;
    }

private:
    std::vector<VehicleCredential> m_credentials;
    std::size_t m_activeIndex = 0;
};

} // namespace ns3
