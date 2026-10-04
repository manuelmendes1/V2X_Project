"""
Duplicados baseados na receção CAM atual.

O identificador físico da observação é:
(receiver node, sender station, packetId).

O campo SEQ do envelope pode ser zero no logger atual e não é usado
como identificador de duplicação.
"""

from __future__ import annotations

from collections import Counter
from dataclasses import dataclass, field

from ..identity import IdentityMap, resolve_sender_receiver
from ..parser import LogEntry


@dataclass
class DuplicateMetrics:
    total_valid_receptions: int = 0
    duplicated_keys: int = 0
    duplicate_count: int = 0

    skipped_invalid_pkt: int = 0
    skipped_unresolved_sender: int = 0

    _counts: Counter[tuple[int, int, int]] = field(
        default_factory=Counter,
        repr=False,
    )

    def top_duplicates(self, limit: int = 10):
        return [
            item
            for item in self._counts.most_common(limit)
            if item[1] > 1
        ]


def compute(
    entries: list[LogEntry],
    identity: IdentityMap,
) -> DuplicateMetrics:

    m = DuplicateMetrics()

    for entry in entries:
        if not entry.is_("CAM_RX", "RECEPTION_OK"):
            continue

        if entry.pkt is None or entry.pkt <= 0:
            m.skipped_invalid_pkt += 1
            continue

        sender, _ = resolve_sender_receiver(entry, identity)
        if sender is None:
            m.skipped_unresolved_sender += 1
            continue

        sender_station = identity.veh_to_station.get(sender)
        if sender_station is None:
            m.skipped_unresolved_sender += 1
            continue

        key = (entry.node, sender_station, entry.pkt)

        m.total_valid_receptions += 1
        m._counts[key] += 1

    m.duplicated_keys = sum(
        1 for count in m._counts.values()
        if count > 1
    )

    m.duplicate_count = sum(
        count - 1
        for count in m._counts.values()
        if count > 1
    )

    return m
