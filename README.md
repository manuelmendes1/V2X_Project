# ns-3 + Vanetza + SUMO V2X Integration

Integration project combining ns-3.47, Vanetza, SUMO and ETSI V2X functionality.

## Scope

This repository contains the project-specific V2X integration layer:

- ETSI CAM generation, encoding, security and reception
- Vanetza BTP-B and GeoNetworking integration
- ns-3 access-layer and DCC integration
- SUMO vehicle synchronization
- PacketContext-based TX/RX tracing and logging
- V2X log analysis and validation tests

The repository does not contain the complete ns-3.47 source tree or generated build artefacts.

## Current TX pipeline

```text
GENERATE -> ENCODE -> SIGN -> BTP_TX -> GEONET_TX -> ACCESS_TX -> SEND -> TX_COMPLETE
```

## Current RX pipeline

```text
RAW_RX -> ACCESS_RX -> GEONET_RX -> BTP_RX -> VERIFY -> DECODE -> REPLAY -> RECEPTION -> DELIVER
```

## Main components

Project code is under scratch/vanetza_integration/.
The simulation entry point is scratch/v2x/v2x_sumo.cc.
The analyzer is under scratch/v2x_analyzer/.

## Documentation

The operational baseline is scratch/vanetza_integration/docs/CURRENT_STATE.md.
The Vanetza architecture reference is scratch/vanetza_integration/docs/vanetza_architecture.md.
Historical material is separated under scratch/vanetza_integration/docs/historical/.

## Build

From an ns-3.47 workspace:

```bash
./ns3 build
```

## Validation

A successful build is not considered proof of functional correctness.
The project distinguishes implementation, build success, simulation execution, demonstrated pipeline behaviour and external interoperability.

## Dependencies

The current build expects an external ns-3.47 workspace, Vanetza, OpenSSL and SUMO.
