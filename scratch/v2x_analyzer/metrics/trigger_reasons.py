"""
Motivo de geração CAM segundo o logger atual.

O evento operacional atual é CAM/GENERATE_OK e o motivo é lido do
campo reason= existente na mensagem.
"""

from __future__ import annotations

from dataclasses import dataclass

from ..parser import LogEntry


_REASON_MAP = {
    "DISTANCE": "distance",
    "SPEED": "speed",
    "HEADING": "heading",
    "TIME": "maxtime",
    "FIRST_CAM": "first_cam",
}


@dataclass
class TriggerReasonMetrics:
    distance: int = 0
    speed: int = 0
    heading: int = 0
    maxtime: int = 0
    first_cam: int = 0

    @property
    def total(self) -> int:
        return (
            self.distance
            + self.speed
            + self.heading
            + self.maxtime
            + self.first_cam
        )


def compute(entries: list[LogEntry]) -> TriggerReasonMetrics:
    m = TriggerReasonMetrics()

    for entry in entries:
        if not entry.is_("CAM", "GENERATE_OK"):
            continue

        reason = entry.kv().get("reason")
        field = _REASON_MAP.get(reason) if reason else None

        if field:
            setattr(m, field, getattr(m, field) + 1)

    return m
