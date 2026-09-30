"""
Métricas RX segundo a taxonomia operacional atual.
"""

from __future__ import annotations

from dataclasses import dataclass

from ..parser import LogEntry


@dataclass
class RxPipelineMetrics:
    raw_rx: int = 0

    access_rx_begin: int = 0
    access_rx_ok: int = 0

    geonet_rx_begin: int = 0
    geonet_rx_ok: int = 0

    btp_rx_begin: int = 0
    btp_rx_ok: int = 0

    verify_begin: int = 0
    verify_ok: int = 0
    verify_fail: int = 0

    decode_ok: int = 0
    decode_fail: int = 0

    replay_ok: int = 0
    replay_drop: int = 0

    reception_begin: int = 0
    reception_ok: int = 0

    deliver_begin: int = 0
    deliver_ok: int = 0


_MAP = {
    ("RX", "RAW_RX"): "raw_rx",

    ("ACCESS", "ACCESS_RX_BEGIN"): "access_rx_begin",
    ("ACCESS", "ACCESS_RX_OK"): "access_rx_ok",

    ("GEONET", "GEONET_RX_BEGIN"): "geonet_rx_begin",
    ("GEONET", "GEONET_RX_OK"): "geonet_rx_ok",

    ("BTP", "BTP_RX_BEGIN"): "btp_rx_begin",
    ("BTP", "BTP_RX_OK"): "btp_rx_ok",

    ("SECURITY", "VERIFY_BEGIN"): "verify_begin",
    ("SECURITY", "VERIFY_OK"): "verify_ok",
    ("SECURITY", "VERIFY_FAIL"): "verify_fail",

    ("CAM", "DECODE_OK"): "decode_ok",
    ("DENM", "DENM_DECODE_OK"): "decode_ok",
    ("CAM", "DECODE_FAIL"): "decode_fail",

    ("REPLAY", "REPLAY_OK"): "replay_ok",
    ("DENM", "DENM_REPLAY_OK"): "replay_ok",
    ("REPLAY", "REPLAY_DROP"): "replay_drop",
    ("DENM", "DENM_REPLAY_DROP"): "replay_drop",

    ("CAM_RX", "RECEPTION_BEGIN"): "reception_begin",
    ("CAM_RX", "RECEPTION_OK"): "reception_ok",
    ("DENM", "DENM_RECEPTION_OK"): "reception_ok",

    ("RX", "DELIVER_BEGIN"): "deliver_begin",
    ("RX", "DELIVER_OK"): "deliver_ok",
    ("DENM", "DENM_DELIVER_OK"): "deliver_ok",
}


def compute(entries: list[LogEntry]) -> RxPipelineMetrics:
    metrics = RxPipelineMetrics()

    for entry in entries:
        field = _MAP.get((entry.module, entry.event))
        if field is not None:
            setattr(metrics, field, getattr(metrics, field) + 1)

    return metrics
