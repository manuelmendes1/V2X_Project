"""
Duração das operações para eventos atuais que transportam DURATION_US.
"""

from __future__ import annotations

from dataclasses import dataclass
from statistics import mean, median

from ..parser import LogEntry


_DURATION_EVENTS = {
    ("CAM", "ENCODE_OK"): "encode",
    ("DENM", "DENM_ENCODE_OK"): "encode",
    ("SECURITY", "SIGN_OK"): "sign",
    ("CAM", "DECODE_OK"): "decode",
    ("DENM", "DENM_DECODE_OK"): "decode",
}


@dataclass
class OperationTiming:
    label: str
    samples: list[int]

    @property
    def n(self) -> int:
        return len(self.samples)

    @property
    def min_us(self) -> int | None:
        return min(self.samples) if self.samples else None

    @property
    def max_us(self) -> int | None:
        return max(self.samples) if self.samples else None

    @property
    def mean_us(self) -> float | None:
        return mean(self.samples) if self.samples else None

    @property
    def median_us(self) -> float | None:
        return median(self.samples) if self.samples else None

    @property
    def p95_us(self) -> float | None:
        if not self.samples:
            return None

        ordered = sorted(self.samples)
        idx = min(int(len(ordered) * 0.95), len(ordered) - 1)
        return ordered[idx]


@dataclass
class TimingMetrics:
    operations: dict[str, OperationTiming]


def compute(entries: list[LogEntry]) -> TimingMetrics:
    buckets: dict[str, list[int]] = {
        "encode": [],
        "sign": [],
        "decode": [],
    }

    for entry in entries:
        label = _DURATION_EVENTS.get((entry.module, entry.event))
        if label is None:
            continue

        duration = entry.duration_us()
        if duration is not None:
            buckets[label].append(duration)

    return TimingMetrics(
        operations={
            label: OperationTiming(label=label, samples=samples)
            for label, samples in buckets.items()
        }
    )
