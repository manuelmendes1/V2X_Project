#pragma once

#include "ns3/node.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/traffic-control-module.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "context/neighbor/neighbor_types.h"
#include "context/vehicle_security/vehicle_security_context.h"
#include "context/vehicle_mobility/vehicle_mobility_context.h"
#include "vanetza_integration/vanetza_runtime/vanetza_runtime.h"


namespace ns3 {

    // ============================================================================
    // Permanent Vehicle Identity
    // ============================================================================
    struct VehicleIdentity
    {
        std::string vehId;
        uint32_t nodeId = 0;
        uint32_t stationId = 0;
    };

    // ============================================================================
    // CAM Trigger Snapshot
    // ============================================================================
    struct CamTriggerSnapshot
    {
        Vector position;
        double speed = 0.0;
        double heading = 0.0;
        double lastCamTime = -1.0;
    };

    // ============================================================================
    // Runtime Vehicle State
    // ============================================================================
    struct VehicleState
    {
        // ------------------------------------------------------------------------
        // Permanent identity
        // ------------------------------------------------------------------------

        VehicleIdentity identity;

        // ------------------------------------------------------------------------
        // ns-3 objects
        // ------------------------------------------------------------------------

        Ptr<Node> node;
        Ptr<Socket> socket;

        // ------------------------------------------------------------------------
        // Simulation state
        // ------------------------------------------------------------------------

        bool initialized = false;

        double lastCamTime = -1.0;
        double lastCamGenTime = -1.0;

        CamTriggerSnapshot lastCamSnapshot;

        // ------------------------------------------------------------------------
        // Mobility
        // ------------------------------------------------------------------------

        VehicleMobilityContext mobility;

        // ------------------------------------------------------------------------
        // CAM sequencing
        // ------------------------------------------------------------------------

        int64_t camSequenceCounter = 0;
        int64_t currentTxSeqId = 0;

        // ------------------------------------------------------------------------
        // Packet identity
        // ------------------------------------------------------------------------

        uint64_t vehiclePacketCounter = 0;

        // ------------------------------------------------------------------------
        // Security
        // ------------------------------------------------------------------------

        VehicleSecurityContext security;

        // ------------------------------------------------------------------------
        // Networking
        // ------------------------------------------------------------------------

        Ipv4Address ipv4Address;
        bool ipv4Initialized = false;

        // ------------------------------------------------------------------------
        // GeoNetworking identity
        // ------------------------------------------------------------------------
        //
        // stationId:
        //   Vehicle identity identifier owned by VehicleIdentityInitializer.
        //
        // GN_ADDR:
        //   ETSI GeoNetworking network-layer address owned/materialized by
        //   VanetzaRuntime.
        //
        // GN_ADDR.MID:
        //   MAC-like identifier carried by GN_ADDR.
        //   It is deterministically derived from stationId.
        //
        // IMPORTANT:
        //   linkMac, stationId, GN_ADDR.MID and GN_ADDR are distinct concepts.
        //   They MUST NOT be treated as interchangeable addresses.
        //
        Mac48Address linkMac;
        bool linkMacInitialized = false;

        std::vector<Ptr<QueueDisc>> queueDiscs;

        std::map<uint32_t, NeighborInfo> neighbors;

        // ------------------------------------------------------------------------
        // ETSI ITS Runtime
        // ------------------------------------------------------------------------

        std::unique_ptr<VanetzaRuntime> runtime;

        // ------------------------------------------------------------------------
        // Construction
        // ------------------------------------------------------------------------

        VehicleState() = default;

        VehicleState(const VehicleState&) = delete;
        VehicleState& operator=(const VehicleState&) = delete;

        VehicleState(VehicleState&&) = delete;
        VehicleState& operator=(VehicleState&&) = delete;

        // ------------------------------------------------------------------------
        // Cleanup
        // ------------------------------------------------------------------------

        ~VehicleState() = default;
    };

} // namespace ns3
