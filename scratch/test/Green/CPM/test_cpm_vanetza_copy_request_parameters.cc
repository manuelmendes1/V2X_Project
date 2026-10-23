#include <vanetza/btp/data_request.hpp>
#include <vanetza/geonet/data_request.hpp>
#include <vanetza/geonet/lifetime.hpp>
#include <vanetza/geonet/mib.hpp>
#include <vanetza/units/time.hpp>

#include <cmath>
#include <iostream>

int main()
{
    using vanetza::btp::DataRequestB;
    using vanetza::geonet::DataRequest;
    using vanetza::geonet::Lifetime;
    using vanetza::geonet::MIB;
    using vanetza::units::Duration;

    const MIB mib{};

    // Confirm the actual MIB defaults before testing overrides.
    DataRequest defaultRequest(mib);
    const double mibLifetimeSeconds =
        defaultRequest.maximum_lifetime.decode().value();
    const unsigned mibHopLimit = defaultRequest.max_hop_limit;

    // Choose a standard, representable lifetime distinct from the MIB
    // default so that the test can detect a missing copy operation.
    const double candidates[] = {0.5, 2.0, 1.0, 4.0};
    double requestedLifetimeSeconds = candidates[0];

    for (double candidate : candidates)
    {
        if (std::abs(candidate - mibLifetimeSeconds) > 0.1)
        {
            requestedLifetimeSeconds = candidate;
            break;
        }
    }

    DataRequestB explicitBtpRequest{};
    Lifetime lifetime;
    lifetime.encode(Duration::from_value(requestedLifetimeSeconds));
    explicitBtpRequest.gn.maximum_lifetime = lifetime;
    explicitBtpRequest.gn.maximum_hop_limit = 7u;

    DataRequest explicitGeoNetRequest(mib);
    vanetza::geonet::copy_request_parameters(
        explicitBtpRequest,
        explicitGeoNetRequest);

    const double copiedLifetimeSeconds =
        explicitGeoNetRequest.maximum_lifetime.decode().value();

    if (std::abs(copiedLifetimeSeconds - requestedLifetimeSeconds) > 1e-6)
    {
        std::cerr << "FAIL EXPLICIT_LIFETIME"
                  << " requested=" << requestedLifetimeSeconds
                  << " copied=" << copiedLifetimeSeconds << '\n';
        return 1;
    }
    std::cout << "PASS EXPLICIT_LIFETIME\n";

    if (explicitGeoNetRequest.max_hop_limit != 7u)
    {
        std::cerr << "FAIL EXPLICIT_HOP_LIMIT"
                  << " expected=7"
                  << " actual=" << explicitGeoNetRequest.max_hop_limit
                  << '\n';
        return 2;
    }
    std::cout << "PASS EXPLICIT_HOP_LIMIT\n";

    // Missing optionals must preserve the defaults established by MIB.
    DataRequestB btpRequestWithoutOverrides{};
    DataRequest geoNetRequestWithoutOverrides(mib);

    vanetza::geonet::copy_request_parameters(
        btpRequestWithoutOverrides,
        geoNetRequestWithoutOverrides);

    const double resultingDefaultLifetime =
        geoNetRequestWithoutOverrides.maximum_lifetime.decode().value();

    if (std::abs(resultingDefaultLifetime - mibLifetimeSeconds) > 1e-6)
    {
        std::cerr << "FAIL MIB_DEFAULT_LIFETIME"
                  << " expected=" << mibLifetimeSeconds
                  << " actual=" << resultingDefaultLifetime << '\n';
        return 3;
    }
    std::cout << "PASS MIB_DEFAULT_LIFETIME\n";

    if (geoNetRequestWithoutOverrides.max_hop_limit != mibHopLimit)
    {
        std::cerr << "FAIL MIB_DEFAULT_HOP_LIMIT"
                  << " expected=" << mibHopLimit
                  << " actual=" << geoNetRequestWithoutOverrides.max_hop_limit
                  << '\n';
        return 4;
    }
    std::cout << "PASS MIB_DEFAULT_HOP_LIMIT\n";

    std::cout << "CPM_VANETZA_COPY_REQUEST_PARAMETERS=PASS\n";
    return 0;
}
