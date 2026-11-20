# E2E Evidence — NS-3 to SUMO Return Dispatcher

**Result: PASS — local end-to-end validation.**

## 1. Validated results

| Indicator | Result |
|---|---:|
| Bridge regression tests | 50/50 PASS |
| Successful UDP sends from ns-3 | 39 |
| Datagrams received by Python bridge | 39 |
| Payload occurrences reconciled | 39/39 |
| Vehicle states accepted by dispatcher | 39 |
| Vehicle states rejected | 0 |
| Invalid UTF-8 datagrams | 0 |
| Missing payloads | 0 |
| Unexpected payloads | 0 |
| ns-3 exit code | 0 |
| Python bridge exit code | 0 |

The original runtime result is preserved in the logs. The payload
reconciliation has also been independently replayed from the saved
`strace` and bridge log.

## 2. Architecture validated

The demonstrated return path is:

`SimulationExport`
→ `SumoAdapter::SendToSumo`
→ `SumoSocket::Send`
→ UDP
→ Python bridge socket
→ `receive_from_ns3`
→ `parse_ns3_vehicle_state`
→ `handle_ns3_vehicle_state`

The Python bridge binds to `127.0.0.1:7000`.
The ns-3 return datagrams are sent to that endpoint.

The opposite direction, SUMO → ns-3, uses the destination
`127.0.0.1:7400`.

## 3. NS-3 return payload contract

The contract is defined by `SumoSerializer::Serialize`.

Exactly six CSV fields are emitted:

`vehicle_id,x,y,speed,heading,accel`

| Field | C++ source |
|---|---|
| vehicle_id | `identity.vehId` |
| x | `mobility.position.x` |
| y | `mobility.position.y` |
| speed | `mobility.speed` |
| heading | `mobility.heading` |
| accel | `mobility.accel` |

This return format has no timestamp, WGS84 coordinates, sequence
number, protocol version or explicit message type.

It is distinct from the seven- and nine-field SUMO → ns-3 vehicle
formats.

## 4. Parser and dispatcher behaviour

`parse_ns3_vehicle_state()`:

- requires exactly six fields;
- rejects an empty vehicle identifier;
- converts the five numeric fields to floating-point values;
- rejects non-numeric and non-finite values.

`receive_from_ns3()`:

- drains available non-blocking UDP datagrams;
- rejects invalid UTF-8 datagrams;
- preserves the established `NS3 -> SUMO | ...` raw log;
- dispatches valid vehicle states;
- logs and skips malformed states without stopping the drain.

`handle_ns3_vehicle_state()` is an observation-only handler. It logs
validated state and does not change SUMO vehicle mobility through TraCI.

## 5. Execution configuration

Scenario:

`sumo/current/scenario/georeferenced/vila-nova-de-famalicao/famalicao-demo.sumocfg`

| Parameter | Value |
|---|---|
| SUMO executable | `sumo` |
| V2X_SUMO_STEP_LENGTH | `0.1` |
| V2X_SUMO_SIM_TIME | `4.0` |
| V2X_TLS_ID | `11645226075` |
| ns-3 simDuration | `45` |
| ns-3 enableCam | `false` |
| UDP send capture | `strace -f -tt -s 4096 -e trace=sendto` |

The processes terminated with exit code zero.

## 6. Acceptance criteria

| Criterion | Result |
|---|---|
| Bridge regression | PASS |
| All sent datagrams received | PASS |
| Exact payload reconciliation | PASS |
| All valid vehicle states dispatched | PASS |
| Unexpected state rejections | None |
| Missing datagrams in this run | None |
| Clean process termination | PASS |

## 7. Limitations

This evidence does not establish a SUMO vehicle-control protocol.

Specifically:

1. The handler does not call `traci.vehicle.moveToXY`,
   `setSpeed`, `setPosition`, `setAngle`, or other mobility mutators.
2. SUMO remains authoritative for scenario vehicle mobility.
3. The return datagram carries no logical SUMO timestamp.
4. Individual return states cannot be correlated with a specific
   SUMO simulation step using this payload alone.
5. The protocol has no acknowledgement, retransmission or sequence
   number, and UDP does not guarantee delivery.
6. The test was conducted on localhost; it does not establish
   behaviour under packet loss, sustained load or distributed execution.
7. No functional actuation of SUMO from ns-3 state has been demonstrated.

The 39/39 result demonstrates complete delivery and processing in
this particular execution. It is not a general UDP delivery guarantee.

## 8. Evidence files

- `ns3.log` — ns-3 process log.
- `bridge.log` — Python bridge log, including received and accepted states.
- `ns3-sendto.strace` — captured sendto calls.
- `bridge-tests.log` — regression output from the original E2E run.
- `bridge-tests-revalidation.log` — regression revalidation at archive time.
- `reconciliation.txt` — independently replayed payload reconciliation.
- `verify_e2e.py` — script for reproducing payload reconciliation.
- `git-head.txt` — repository HEAD recorded during evidence capture.
- `git-branch.txt` — branch recorded during evidence capture.
- `git-status-at-capture.txt` — worktree status recorded during evidence capture.
- `source-sha256.txt` — hashes of relevant source files and executable.
- `environment.txt` — host and tool environment information.
- `execution-parameters.txt` — simulation parameters.

## 9. Revalidation

Run from this directory:

`python3 verify_e2e.py`

Expected result:

- `NS3_SENDTO_SUCCESS_COUNT=39`
- `BRIDGE_RECEIVE_LOG_COUNT=39`
- `PAYLOAD_OCCURRENCES_MATCHED=39`
- `PAYLOAD_OCCURRENCES_MISSING=0`
- `UNEXPECTED_RECEIVE_OCCURRENCES=0`
- `DISPATCHER_ACCEPTED_COUNT=39`
- `DISPATCHER_REJECTED_COUNT=0`
- `UTF8_REJECTED_COUNT=0`
- `UDP_PAYLOAD_RECONCILIATION=PASS`
- `NS3_TO_BRIDGE_DISPATCH_E2E=PASS`

## 10. Integrity and scope

`MANIFEST.sha256` contains SHA-256 hashes of the archived evidence files.
`source-sha256.txt` identifies the source files and executable captured
when this evidence package was prepared.

This report records one local execution. It does not change the overall
completion status of the VAM, CPM, SPATEM or wider ns-3 / Vanetza / SUMO
integration stages.
