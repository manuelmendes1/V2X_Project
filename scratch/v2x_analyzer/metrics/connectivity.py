"""
Conectividade e entrega efetiva segundo DELIVER_OK.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from ..identity import IdentityMap, resolve_sender_receiver
from ..parser import LogEntry


@dataclass
class ConnectivityMetrics:
    n_vehicles: int = 0
    max_pairs: int = 0

    active_pairs: set[tuple[str, str]] = field(default_factory=set)

    tx_complete: int = 0
    deliver_ok: int = 0

    @property
    def n_active_pairs(self) -> int:
        return len(self.active_pairs)

    @property
    def theoretical_max_receptions(self) -> int:
        return self.tx_complete * max(self.n_vehicles - 1, 0)

    @property
    def effective_delivery_pct(self) -> float | None:
        maximum = self.theoretical_max_receptions
        if maximum == 0:
            return None
        return 100.0 * self.deliver_ok / maximum

    @property
    def avg_fanout(self) -> float | None:
        if self.tx_complete == 0:
            return None
        return self.deliver_ok / self.tx_complete

    def missing_pairs(self) -> list[tuple[str, str]]:
        vehicles = sorted(identity_vehicles(self._identity))
        missing: list[tuple[str, str]] = []

        for sender in vehicles:
            for receiver in vehicles:
                if sender != receiver and (sender, receiver) not in self.active_pairs:
                    missing.append((sender, receiver))

        return missing

    _identity: IdentityMap | None = field(default=None, repr=False)


def identity_vehicles(identity: IdentityMap) -> set[str]:
    return set(identity.veh_to_node)


def compute(
    entries: list[LogEntry],
    identity: IdentityMap,
    tx_complete: int,
) -> ConnectivityMetrics:

    metrics = ConnectivityMetrics()
    metrics.n_vehicles = identity.n_vehicles
    metrics.max_pairs = (
        identity.n_vehicles * (identity.n_vehicles - 1)
        if identity.n_vehicles
        else 0
    )
    metrics.tx_complete = tx_complete
    metrics._identity = identity

    for entry in entries:
        if not entry.is_("RX", "DELIVER_OK"):
            continue

        metrics.deliver_ok += 1

        sender, receiver = resolve_sender_receiver(entry, identity)

        if (
            sender is not None
            and receiver is not None
            and sender != receiver
        ):
            metrics.active_pairs.add((sender, receiver))

    return metrics
