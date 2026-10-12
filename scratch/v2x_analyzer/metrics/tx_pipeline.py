"""
Métricas TX segundo a taxonomia operacional atual.
"""

from __future__ import annotations

from dataclasses import dataclass

from ..parser import LogEntry


@dataclass
class TxPipelineMetrics:
    generate_ok: int = 0

    encode_begin: int = 0
    encode_ok: int = 0
    encode_fail: int = 0

    sign_begin: int = 0
    sign_ok: int = 0
    sign_fail: int = 0

    btp_tx_begin: int = 0
    btp_tx_ok: int = 0

    geonet_tx_begin: int = 0
    geonet_tx_ok: int = 0

    access_tx_begin: int = 0
    access_tx_ok: int = 0

    send_begin: int = 0
    send_ok: int = 0
    send_fail: int = 0

    tx_complete: int = 0

    @property
    def encode_attempts(self) -> int:
        return self.encode_ok + self.encode_fail

    @property
    def encode_fail_rate(self) -> float | None:
        if self.encode_attempts == 0:
            return None
        return 100.0 * self.encode_fail / self.encode_attempts


_MAP = {
    ("CAM", "GENERATE_OK"): "generate_ok",
    ("SPATEM", "SPATEM_GENERATE_OK"): "generate_ok",
    ("DENM", "GENERATE_OK"): "generate_ok",

    ("CAM", "ENCODE_BEGIN"): "encode_begin",
    ("CAM", "ENCODE_OK"): "encode_ok",
    ("SPATEM", "SPATEM_ENCODE_OK"): "encode_ok",
    ("DENM", "ENCODE_OK"): "encode_ok",
    ("CAM", "ENCODE_FAIL"): "encode_fail",

    ("SECURITY", "SIGN_BEGIN"): "sign_begin",
    ("SECURITY", "SIGN_OK"): "sign_ok",
    ("SECURITY", "SIGN_FAIL"): "sign_fail",

    ("BTP", "BTP_TX_BEGIN"): "btp_tx_begin",
    ("BTP", "BTP_TX_OK"): "btp_tx_ok",

    ("GEONET", "GEONET_TX_BEGIN"): "geonet_tx_begin",
    ("GEONET", "GEONET_TX_OK"): "geonet_tx_ok",

    ("ACCESS", "ACCESS_TX_BEGIN"): "access_tx_begin",
    ("ACCESS", "ACCESS_TX_OK"): "access_tx_ok",

    ("ACCESS", "SEND_BEGIN"): "send_begin",
    ("ACCESS", "SEND_OK"): "send_ok",
    ("ACCESS", "SEND_FAIL"): "send_fail",

    ("TX", "TX_COMPLETE"): "tx_complete",
}


def compute(entries: list[LogEntry]) -> TxPipelineMetrics:
    metrics = TxPipelineMetrics()

    for entry in entries:
        field = _MAP.get((entry.module, entry.event))
        if field is not None:
            setattr(metrics, field, getattr(metrics, field) + 1)

    return metrics
