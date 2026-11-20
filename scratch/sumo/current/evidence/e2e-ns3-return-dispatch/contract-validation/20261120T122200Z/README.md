# Supplementary Validation — NS-3 / Python Vehicle-State Contract

Date: 2026-11-20
Result: PASS

## 1. Relationship with the original E2E

This is a supplementary validation performed after the original
NS-3 → Python bridge E2E evidence was archived.

Original E2E evidence:

`../20261120T121453Z/README.md`

The original runtime campaign remains unchanged:

- 39 successful UDP sends.
- 39 datagrams received.
- 39 payload occurrences reconciled.
- 39 states accepted by the dispatcher.
- 0 missing payloads.
- 0 rejected states.
- Original bridge regression: 50/50 PASS.

This supplementary validation does not replace or rewrite those
historical runtime results.

## 2. Additional cross-language tests

Three new tests verify:

1. Python dataclass field order.
2. C++ serializer source-field order.
3. Python parser mapping from CSV positions to typed fields.

The tests use the six-field contract:

`vehicle_id,x,y,speed,heading,accel`

Results:

- Cross-language contract tests: 3/3 PASS.
- Complete bridge regression: 53/53 PASS.

See:

- `cross-language-tests.log`
- `bridge-regression.log`

## 3. Scope

The C++/Python contract test performs source-level inspection of the
C++ serializer and tests the Python parser with distinguishable values.

It does not compile or invoke the C++ serializer as a standalone test.
The original E2E campaign supplies the separate runtime evidence for
the compiled ns-3 serializer, UDP transport and Python dispatcher.

No production implementation change is part of this supplementary
validation. No new runtime E2E campaign was performed.

## 4. Integrity and provenance

- `git-head.txt`: repository HEAD at validation time.
- `git-branch.txt`: branch at validation time.
- `git-status-at-validation.txt`: worktree status.
- `source-sha256.txt`: relevant source and executable hashes.
- `environment.txt`: host and tool versions.
- `MANIFEST.sha256`: SHA-256 hashes of this supplementary package.

## 5. Revalidation

From this directory:

```bash
python3 -m unittest sumo.current.integration.tests.test_v2x_bridge_16_cpp_python_contract -v

python3 -m unittest discover \
    -s sumo/current/integration/tests \
    -p 'test_v2x_bridge_16*.py' \
    -v
```

Expected results: 3/3 and 53/53 tests pass respectively.
