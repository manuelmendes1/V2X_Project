"""
Identidade no checkout atual.

A fonte primária é VEHICLE/IDENTITY_READY.
Eventos RX atuais também carregam sender/receiver explicitamente.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .parser import LogEntry


@dataclass
class IdentityMap:
    veh_to_node: dict[str, int] = field(default_factory=dict)
    veh_to_station: dict[str, int] = field(default_factory=dict)

    node_to_veh: dict[int, str] = field(default_factory=dict)
    station_to_veh: dict[int, str] = field(default_factory=dict)

    @property
    def n_vehicles(self) -> int:
        return len(self.veh_to_node)


def build_identity_map(entries: list[LogEntry]) -> IdentityMap:
    im = IdentityMap()

    for entry in entries:
        if entry.is_("VEHICLE", "IDENTITY_READY") and entry.veh:
            im.veh_to_node[entry.veh] = entry.node
            im.veh_to_station[entry.veh] = entry.station
            im.node_to_veh[entry.node] = entry.veh
            im.station_to_veh[entry.station] = entry.veh

    return im


def resolve_sender_receiver(
    entry: LogEntry,
    identity: IdentityMap,
) -> tuple[str | None, str | None]:
    """
    Resolve a relação emissor -> recetor usando os campos explícitos
    presentes nos eventos atuais e, quando necessário, a identidade.

    RECEPTION_OK:
      sender=...
      receiver=...

    DELIVER_OK:
      sender=...
      o VEH/NODE do evento identifica o recetor.
    """

    kv = entry.kv()

    sender = kv.get("sender")
    receiver = kv.get("receiver")

    if not sender:
        sender_station = kv.get("sender_station")
        if sender_station is not None:
            try:
                sender = identity.station_to_veh.get(int(sender_station))
            except ValueError:
                sender = None

    if not receiver:
        receiver = entry.veh or identity.node_to_veh.get(entry.node)

    return sender, receiver
