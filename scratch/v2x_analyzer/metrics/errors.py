"""
Falhas segundo os eventos concretos atualmente reconhecidos.
"""

from __future__ import annotations

from dataclasses import dataclass

from ..parser import LogEntry


@dataclass
class ErrorMetrics:
    encode_fail: int = 0
    sign_fail: int = 0
    send_fail: int = 0
    verify_fail: int = 0
    decode_fail: int = 0
    replay_drop: int = 0

    @property
    def total(self) -> int:
        return (
            self.encode_fail
            + self.sign_fail
            + self.send_fail
            + self.verify_fail
            + self.decode_fail
            + self.replay_drop
        )


_MAP = {
    ("CAM", "ENCODE_FAIL"): "encode_fail",
    ("SECURITY", "SIGN_FAIL"): "sign_fail",
    ("ACCESS", "SEND_FAIL"): "send_fail",
    ("SECURITY", "VERIFY_FAIL"): "verify_fail",
    ("CAM", "DECODE_FAIL"): "decode_fail",
    ("REPLAY", "REPLAY_DROP"): "replay_drop",
}


def compute(entries: list[LogEntry]) -> ErrorMetrics:
    m = ErrorMetrics()

    for entry in entries:
        field = _MAP.get((entry.module, entry.event))
        if field is not None:
            setattr(m, field, getattr(m, field) + 1)

    return m
