#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/vehicle_security/vehicle_security_initializer/vehicle_security_initializer.h"
#include "vanetza_integration/context/vehicle_state.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"

#include "vanetza_integration/denm/lifecycle/denm_lifecycle_manager.h"
#include "vanetza_integration/denm/trigger/denm_trigger.h"

#include "vanetza_integration/tests/denm_test_pki.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"

#include "ns3/callback.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-module.h"

#include <iostream>
#include <string>

namespace
{

bool
Check(
    bool condition,
    const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }

    std::cout << "PASS " << message << '\n';
    return true;
}

ns3::DenmTriggerRequest
MakeRequest(
    const std::string& vehicleId,
    const std::string& eventId,
    ns3::DenmTriggerEventType eventType)
{
    ns3::DenmTriggerRequest request;

    request.source = ns3::DenmTriggerSource::TEST;
    request.eventType = eventType;
    request.vehicleId = vehicleId;
    request.eventId = eventId;

    return request;
}

} // namespace

int
main()
{
    std::cout
        << "===== DENM IDENTITY / CREDENTIAL / ACTION POLICY TEST =====\n";

    const std::string pkiDir =
        ns3::denm_test::ResolveDenmPkiDirectory();

    if (pkiDir.empty())
    {
        std::cerr
            << "FAIL DENM test PKI directory is unavailable\n";
        return 1;
    }

    bool ok = true;

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory(
        pkiDir);

    SimulationContext::itsStartTime =
        "2026-09-24T13:38:21";

    ns3::DenmLifecycleManager lifecycle;
    lifecycle.Reset();

    auto& registry =
        ns3::VehicleRegistry::Instance();

    registry.Clear();

    auto channel =
        ns3::YansWifiChannelHelper::Default().Create();

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase(
        "10.0.0.0",
        "255.255.255.0");

    ns3::VehicleFactory::Initialize(
        channel,
        ipv4,
        ns3::MakeNullCallback<
            void,
            ns3::Ptr<ns3::Socket>>());

    ns3::VehicleFactory::CreateVehicle("veh0");

    ns3::VehicleState* vehicle =
        registry.GetByVehId("veh0");

    ok &= Check(
        vehicle != nullptr,
        "veh0 is created through the real VehicleFactory");

    if (vehicle == nullptr)
    {
        return 1;
    }

    const uint32_t stationIdBeforeRotation =
        vehicle->identity.stationId;

    ok &= Check(
        stationIdBeforeRotation != 0,
        "veh0 has a valid authoritative stationID");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetActiveCredential() != nullptr &&
            vehicle->security.denmCredentialPool.GetActiveCredential()->vehicleId ==
                "veh0",
        "initial active credential belongs to veh0");

    // ------------------------------------------------------------
    // P12 RED: deliberately place a valid veh1 credential into
    // veh0's pool. Cryptographic binding is valid; logical ownership
    // is not.
    // ------------------------------------------------------------

    const ns3::VehicleCredential veh0Credential{
        "veh0-ticket-000",
        "veh0",
        0,
        pkiDir + "/veh0-ticket-000.cert",
        pkiDir + "/veh0-ticket-000.pem"};

    const ns3::VehicleCredential foreignCredential{
        "veh1-ticket-000",
        "veh1",
        0,
        pkiDir + "/veh1-ticket-000.cert",
        pkiDir + "/veh1-ticket-000.pem"};

    vehicle->security.denmCredentialPool.SetCredentials({
        veh0Credential,
        foreignCredential});

    ok &= Check(
        vehicle->security.denmCredentialPool.Select(0),
        "veh0 credential is restored as active before contamination test");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetCredential(1) != nullptr &&
            vehicle->security.denmCredentialPool.GetCredential(1)->vehicleId ==
                "veh1",
        "foreign credential explicitly identifies veh1");

    const bool rejectedForeignRotation =
        vehicle->runtime != nullptr &&
        !vehicle->runtime->RotateDenmCredential();

    ok &= Check(
        rejectedForeignRotation,
        "rotation rejects a credential belonging to another vehicle");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetActiveCredentialIndex() == 0,
        "rejected foreign credential preserves active credential index");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetActiveCredential() != nullptr &&
            vehicle->security.denmCredentialPool.GetActiveCredential()->vehicleId ==
                vehicle->identity.vehId,
        "active credential still belongs to veh0 after rejection");

    ok &= Check(
        vehicle->identity.stationId == stationIdBeforeRotation,
        "stationID is unchanged after rejected credential rotation");

    // ------------------------------------------------------------
    // Restore a valid veh0-only pool and perform a legitimate
    // credential rotation.
    // ------------------------------------------------------------

    const ns3::VehicleCredential veh0Credential001{
        "veh0-ticket-001",
        "veh0",
        1,
        pkiDir + "/veh0-ticket-001.cert",
        pkiDir + "/veh0-ticket-001.pem"};

    vehicle->security.denmCredentialPool.SetCredentials({
        veh0Credential,
        veh0Credential001});

    ok &= Check(
        vehicle->runtime != nullptr &&
            vehicle->runtime->RotateDenmCredential(),
        "legitimate veh0 credential rotation succeeds");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetActiveCredentialIndex() == 1,
        "credential sequence 001 becomes active");

    ok &= Check(
        vehicle->security.denmCredentialPool.GetActiveCredential() != nullptr &&
            vehicle->security.denmCredentialPool.GetActiveCredential()->vehicleId ==
                vehicle->identity.vehId,
        "rotated active credential still belongs to veh0");

    ok &= Check(
        vehicle->identity.stationId == stationIdBeforeRotation,
        "stationID remains unchanged after legitimate credential rotation");

    // ------------------------------------------------------------
    // ActionID belongs to the DENM lifecycle/origin station, not
    // to the credential sequence.
    // ------------------------------------------------------------

    DenmLifecycleResult firstResult;

    ok &= Check(
        lifecycle.Process(
            *vehicle,
            MakeRequest(
                "veh0",
                "p12-event-A",
                ns3::DenmTriggerEventType::HAZARD_DETECTED),
            firstResult),
        "first DENM lifecycle event is accepted");

    const DenmActionId firstActionId =
        firstResult.actionId;

    ok &= Check(
        firstActionId.originatingStationId ==
            stationIdBeforeRotation,
        "ActionID origin equals authoritative stationID");

    ok &= Check(
        firstActionId.sequenceNumber != 0,
        "first ActionID has a valid sequence");

    DenmLifecycleResult updateResult;

    ok &= Check(
        lifecycle.Process(
            *vehicle,
            MakeRequest(
                "veh0",
                "p12-event-A",
                ns3::DenmTriggerEventType::HAZARD_UPDATED),
            updateResult),
        "same DENM lifecycle updates after credential rotation");

    ok &= Check(
        updateResult.actionId.originatingStationId ==
                firstActionId.originatingStationId &&
            updateResult.actionId.sequenceNumber ==
                firstActionId.sequenceNumber,
        "credential rotation does not change the active lifecycle ActionID");

    DenmLifecycleResult secondResult;

    ok &= Check(
        lifecycle.Process(
            *vehicle,
            MakeRequest(
                "veh0",
                "p12-event-B",
                ns3::DenmTriggerEventType::HAZARD_DETECTED),
            secondResult),
        "independent DENM lifecycle event is accepted");

    ok &= Check(
        secondResult.actionId.originatingStationId ==
            stationIdBeforeRotation,
        "independent ActionID keeps the same originating station");

    ok &= Check(
        secondResult.actionId.sequenceNumber !=
            firstActionId.sequenceNumber,
        "independent DENM event receives a different ActionID sequence");

    lifecycle.Reset();
    registry.Clear();

    ns3::VehicleSecurityInitializer::SetDenmPkiDirectory("");
    SimulationContext::itsStartTime.clear();

    std::cout
        << "DENM_IDENTITY_CREDENTIAL_ACTION_POLICY_EXIT="
        << (ok ? 0 : 1)
        << '\n';

    return ok ? 0 : 1;
}
