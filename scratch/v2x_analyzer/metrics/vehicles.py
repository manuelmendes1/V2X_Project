"""
Métricas TX/RX por veículo usando apenas eventos atuais.
"""

from __future__ import annotations

from collections import Counter
from dataclasses import dataclass, field

from ..identity import IdentityMap, resolve_sender_receiver
from ..parser import LogEntry


@dataclass
class VehicleMetrics:
    tx_by_vehicle: Counter[str] = field(default_factory=Counter)
    rx_by_vehicle: Counter[str] = field(default_factory=Counter)
    tx_success_by_vehicle: Counter[str] = field(default_factory=Counter)

    def ghost_senders(self) -> list[str]:
        return sorted(
            v
            for v, count in self.tx_by_vehicle.items()
            if count > 0 and self.tx_success_by_vehicle[v] == 0
        )


def compute(
    entries: list[LogEntry],
    identity: IdentityMap,
) -> VehicleMetrics:

    metrics = VehicleMetrics()

    for entry in entries:
        if entry.is_("TX", "TX_COMPLETE"):
            if entry.veh:
                metrics.tx_by_vehicle[entry.veh] += 1

        elif (
            entry.is_("RX", "DELIVER_OK")
            or entry.is_("DENM", "DENM_DELIVER_OK")
        ):
            sender, receiver = resolve_sender_receiver(entry, identity)

            if sender:
                metrics.tx_success_by_vehicle[sender] += 1

            if receiver:
                metrics.rx_by_vehicle[receiver] += 1

    return metrics
