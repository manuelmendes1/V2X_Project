# Etapa 11.5.2 — SUMO Person Controller Ingress (RED)

## Baseline and scope

Baseline: `44d1dd7` (`refactor(v2x): extract neutral ITS station foundation`).

This patch adds tests only. It does not modify production code, the SUMO bridge,
CMake, generated evidence, or existing tests.

The preceding GREEN work already provides:

- the `SUMO_PERSON,UPSERT,...` and `SUMO_PERSON,REMOVE,...` datagrams;
- `SumoPersonDatagram` parsing;
- a dedicated `SumoAdapter::PersonUpdateCallback`.

The remaining gap is at the application boundary: `SimulationController`
does not register that callback or retain the latest person observations.

## Contract under test

A new `SumoPersonRegistry` in `context/sumo_person_registry` will maintain
SUMO-person observations keyed only by `personId`:

- `UPSERT` requires a valid mobility sample and stores the latest sample and
  SUMO logical timestamp;
- a later `UPSERT` replaces the previous sample for the same ID;
- `REMOVE` deletes only the matching active person and is idempotent for an
  unknown/already removed ID;
- events older than the last accepted event for that ID are rejected, including
  stale `UPSERT` after `REMOVE`, so a delayed UDP datagram cannot resurrect an
  already departed person;
- invalid event/sample combinations are rejected without mutating the registry;
- `Clear()` resets active state and per-ID event-time history for a new run.

`SimulationController` will register `HandlePersonUpdateFromSumo`, apply each
accepted datagram to the registry, clear the registry at simulation start, and
expose it via a const accessor for the later VRU integration stage.

The registry and controller handler must not create `VehicleState`, ns-3
`Node`, `ItsStationState`, station IDs, GN addresses, or VAM runtimes.

## RED tests

```bash
python3 scratch/test/Red/SUMO/test_sumo_person_registry.py
python3 scratch/test/Red/SUMO/test_sumo_person_controller_wiring.py
```

Expected RED result against the current code:

- registry test fails with `SUMO_PERSON_REGISTRY_API_MISSING` because the
  production registry files do not exist;
- controller wiring tests fail because callback registration, handler,
  read-only accessor, registry lifecycle and CMake source wiring are absent.

No production changes are included by this patch. Keep the tests untracked until
the RED results have been reviewed. Do not commit at this stage.
