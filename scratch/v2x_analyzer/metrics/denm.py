"""DENM-specific metrics for lifecycle and V3 signing evidence."""

from __future__ import annotations

from dataclasses import dataclass, field
import re
from typing import Iterable, Sequence


_INT_RE_TEMPLATE = r"\b{key}=(-?\d+)\b"

_LIFECYCLE_EVENTS = frozenset(
    {
        "DENM_DECODE_OK",
        "DENM_REPLAY_OK",
        "DENM_RECEPTION_OK",
        "DENM_DELIVER_OK",
    }
)


@dataclass(frozen=True)
class DenmLifecycleMetrics:
    """Logical DENM lifecycle observations, deduplicated by event/action."""

    new: int = 0
    update: int = 0
    cancellation: int = 0
    logical_events: set[tuple[int, int]] = field(default_factory=set)


@dataclass(frozen=True)
class DenmCredentialMetrics:
    """Structured DENM V3 signing evidence."""

    denm_v3_signatures: int = 0
    denm_v3_aids: set[int] = field(default_factory=set)
    signers: set[str] = field(default_factory=set)
    real_signers: set[str] = field(default_factory=set)
    certificate_paths: dict[str, str] = field(default_factory=dict)


@dataclass(frozen=True)
class DenmReport:
    """Aggregated DENM-specific analyzer metrics."""

    lifecycle: DenmLifecycleMetrics
    credentials: DenmCredentialMetrics


def _extract_int(msg: str, key: str) -> int | None:
    match = re.search(_INT_RE_TEMPLATE.format(key=re.escape(key)), msg)
    return int(match.group(1)) if match else None


def compute_lifecycle(entries: Iterable[object]) -> DenmLifecycleMetrics:
    """Compute logical DENM NEW/UPDATE/CANCELLATION observations.

    Multiple pipeline stages and repeated physical observations of the same
    logical event are collapsed to one logical key:
    (originatingStationId, ActionID).

    The action value identifies the observed lifecycle state:
    0=NEW, 1=UPDATE, 2=CANCELLATION.
    """
    observations: set[tuple[tuple[int, int], int]] = set()

    for entry in entries:
        module = getattr(entry, "module", None)
        event = getattr(entry, "event", None)
        if module != "DENM" or event not in _LIFECYCLE_EVENTS:
            continue

        msg = getattr(entry, "msg", "") or ""
        origin_station = _extract_int(msg, "origin_station")
        action_seq = _extract_int(msg, "action_seq")
        action = _extract_int(msg, "action")

        if origin_station is None or action_seq is None or action is None:
            continue

        logical_key = (origin_station, action_seq)
        observations.add((logical_key, action))

    logical_events = {key for key, _action in observations}

    return DenmLifecycleMetrics(
        new=sum(action == 0 for _key, action in observations),
        update=sum(action == 1 for _key, action in observations),
        cancellation=sum(action == 2 for _key, action in observations),
        logical_events=logical_events,
    )


def compute_credentials(
    entries: Iterable[object],
    provisioning: Sequence[object] | None = None,
) -> DenmCredentialMetrics:
    """Compute structured DENM V3 signing evidence.

    Only SECURITY/SIGN_OK entries explicitly identifying DENM_V3 with
    protocol_version=3 and signed=1 are counted.
    """
    signatures = 0
    aids: set[int] = set()
    signers: set[str] = set()
    real_signers: set[str] = set()
    certificate_paths: dict[str, str] = {}

    for entry in entries:
        if (
            getattr(entry, "module", None) != "SECURITY"
            or getattr(entry, "event", None) != "SIGN_OK"
        ):
            continue

        msg = getattr(entry, "msg", "") or ""
        if not re.search(r"\bDENM_V3\b", msg):
            continue
        if _extract_int(msg, "protocol_version") != 3:
            continue
        if _extract_int(msg, "signed") != 1:
            continue

        signatures += 1

        aid = _extract_int(msg, "aid")
        if aid is not None:
            aids.add(aid)

        veh = getattr(entry, "veh", None)
        if veh:
            signers.add(veh)

    for evidence in provisioning or ():
        if (
            getattr(evidence, "role", None) != "signer"
            or not getattr(evidence, "vehicle", None)
            or not getattr(evidence, "cert_path", None)
        ):
            continue

        vehicle = evidence.vehicle
        real_signers.add(vehicle)
        certificate_paths[vehicle] = evidence.cert_path

    return DenmCredentialMetrics(
        denm_v3_signatures=signatures,
        denm_v3_aids=aids,
        signers=signers,
        real_signers=real_signers,
        certificate_paths=certificate_paths,
    )
