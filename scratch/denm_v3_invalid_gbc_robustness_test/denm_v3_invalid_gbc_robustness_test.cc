#include "vanetza_integration/context/vehicle_factory/vehicle_factory.h"
#include "vanetza_integration/context/vehicle_registry/vehicle_registry.h"
#include "vanetza_integration/context/simulation_context/simulation_context.h"
#include "vanetza_integration/wrappers/vanetza/btp/btp_tx_request.h"
#include "vanetza_integration/context/packet/packet_context.h"

#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-module.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{

bool
Check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << "\n";
        return false;
    }

    std::cout << "PASS " << message << "\n";
    return true;
}

} // namespace

int
main()
{
    bool ok = true;

    SimulationContext::itsStartTime =
        "2026-09-24T13:38:21";

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
        "veh0 created through real VehicleFactory");

    if (vehicle == nullptr)
    {
        return 1;
    }

    ok &= Check(
        vehicle->runtime != nullptr &&
            vehicle->runtime->IsInitialized(),
        "VanetzaRuntime initialized");

    ns3::BtpTxRequest request{};
    request.destinationPort = 2002;
    request.destinationPortInfo = 0;
    request.transport = ns3::BtpTransportType::GBC;
    request.communicationProfile =
        ns3::BtpCommunicationProfile::ITS_G5;
    request.itsAid = 37;
    request.trafficClass = 0;

    ns3::BtpGeoArea invalidArea{};
    invalidArea.latitudeDeg = 91.0;
    invalidArea.longitudeDeg = 0.0;
    invalidArea.radiusMeters = 100.0;
    request.destinationArea = invalidArea;

    ns3::PacketContext context{};

    const bool sent =
        vehicle->runtime->SendBtpB(
            context,
            request,
            std::vector<uint8_t>{0x01});

    ok &= Check(
        !sent,
        "invalid GBC latitude is rejected");

    ok &= Check(
        context.pipelineResult ==
            ns3::PipelineResult::FAIL,
        "invalid GBC marks PacketContext as FAIL");

    ok &= Check(
        context.reason == "invalid_gbc_area",
        "invalid GBC exposes invalid_gbc_area reason");

    ok &= Check(
        context.pipelineStage !=
            ns3::PipelineStage::GEONET_TX,
        "invalid GBC does not advance to GEONET_TX");

    request.destinationArea->latitudeDeg = 0.0;
    request.destinationArea->longitudeDeg = 181.0;

    ns3::PacketContext longitudeContext{};

    const bool longitudeSent =
        vehicle->runtime->SendBtpB(
            longitudeContext,
            request,
            std::vector<uint8_t>{0x01});

    ok &= Check(
        !longitudeSent,
        "invalid GBC longitude is rejected");

    ok &= Check(
        longitudeContext.pipelineResult ==
            ns3::PipelineResult::FAIL,
        "invalid GBC longitude marks PacketContext as FAIL");

    ok &= Check(
        longitudeContext.reason == "invalid_gbc_area",
        "invalid GBC longitude exposes invalid_gbc_area reason");

    ok &= Check(
        longitudeContext.pipelineStage !=
            ns3::PipelineStage::GEONET_TX,
        "invalid GBC longitude does not advance to GEONET_TX");

    std::cout
        << "P21_6_INVALID_GBC_GREEN_EXIT="
        << (ok ? 0 : 1)
        << "\n";

    return ok ? 0 : 1;
}
