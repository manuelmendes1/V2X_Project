# V2X Project Architecture

## Overview

The repository integrates **ns-3.47**, **Vanetza**, **SUMO**, and ETSI V2X facilities. A simulation application synchronizes mobility from SUMO, exchanges ETSI messages through Vanetza and ns-3, and produces packet logs for analysis and validation.

```mermaid
flowchart LR
    subgraph External[External Dependencies]
        SUMO[SUMO Mobility]
        NS3[ns-3.47]
        VAN[Vanetza]
        OSSL[OpenSSL]
    end

    subgraph Build[Build and Entry Points]
        CMAKE[scratch/CMakeLists.txt]
        SIM[scratch/v2x/v2x_sumo.cc]
        CMAKE --> SIM
    end

    subgraph Integration[V2X Integration Layer]
        ADAPT[SUMO Adapter]
        CORE[Core and Context]
        RT[Vanetza Runtime]

        MSG[CAM / DENM / CPM / SPATEM / VAM / VRU]
        SEC[Security and Signing]
        RX[Reception and Decoding]

        CORE --> RT
        RT --> MSG
        RT --> SEC
        RT --> RX
        ADAPT --> CORE
    end

    subgraph Pipeline[Packet Pipelines]
        TX[GENERATE -> ENCODE -> SIGN -> BTP_TX -> GEONET_TX -> ACCESS_TX -> SEND -> TX_COMPLETE]
        RXPIPE[RAW_RX -> ACCESS_RX -> GEONET_RX -> BTP_RX -> VERIFY -> DECODE -> REPLAY -> RECEPTION -> DELIVER]
    end

    subgraph Validation[Validation and Output]
        TESTS[scratch/test<br/>Green / Red suites]
        ANALYZER[scratch/v2x_analyzer]
        LOGS[PacketContext Logs and Evidence]
        TESTS --> LOGS
        ANALYZER --> LOGS
    end

    SUMO -->|vehicle state| ADAPT
    SIM --> ADAPT
    SIM --> CORE

    NS3 -->|access and DCC| RT
    VAN -->|BTP / GeoNetworking| RT
    OSSL -->|signing and verification| SEC

    MSG --> TX
    SEC --> TX
    TX -->|transmit| NS3

    NS3 -->|receive| RXPIPE
    RXPIPE --> RX
    RX --> ANALYZER

    TX --> LOGS
    RXPIPE --> LOGS
    TESTS -->|run| SIM
```

## Major Components

- **Simulation entry point:** `scratch/v2x/v2x_sumo.cc` coordinates the SUMO-based scenario and ns-3 simulation.
- **Vanetza integration:** `scratch/vanetza_integration/` contains CAM, DENM, CPM, SPATEM, VAM, and VRU handling, plus runtime, security, context, reception, and SUMO adapters.
- **Message processing:** generated ETSI facilities are encoded, signed, sent through BTP and GeoNetworking, received, verified, decoded, and replayed.
- **SUMO synchronization:** the SUMO adapter supplies vehicle state and scenario mobility to the simulation.
- **Analyzer:** `scratch/v2x_analyzer/` parses V2X logs and computes validation metrics and reports.
- **Tests:** `scratch/test/` contains positive and negative CAM, DENM, and SPATEM suites.
- **Evidence:** runtime logs and collected evidence support functional validation.

## TX Flow

```mermaid
sequenceDiagram
    participant App as V2X Application
    participant SEC as Security
    participant VAN as Vanetza BTP/GeoNetworking
    participant NS3 as ns-3 Access/DCC

    App->>App: Generate ETSI facility message
    App->>App: Encode
    App->>SEC: Sign
    SEC-->>App: Signed payload
    App->>VAN: BTP_TX
    VAN->>NS3: GEONET_TX / ACCESS_TX
    NS3-->>App: SEND / TX_COMPLETE
```

## RX Flow

```mermaid
sequenceDiagram
    participant NS3 as ns-3 Access/DCC
    participant VAN as Vanetza BTP/GeoNetworking
    participant SEC as Security
    participant APP as V2X Application

    NS3->>VAN: RAW_RX / ACCESS_RX
    VAN->>VAN: GEONET_RX / BTP_RX
    VAN->>SEC: VERIFY
    SEC-->>APP: Verified payload
    APP->>APP: DECODE / REPLAY / RECEPTION
    APP-->>APP: DELIVER
```
