"""
Eventos canónicos da taxonomia atual do logger V2X.

Não contém aliases históricos. O checkout atual tem uma única taxonomia
operacional e o analyzer deve analisar essa taxonomia diretamente.
"""

from __future__ import annotations

TX_TERMINAL = frozenset({
    ("TX", "TX_COMPLETE"),
})

RX_TERMINAL = frozenset({
    ("RX", "DELIVER_OK"),
    ("DENM", "DENM_DELIVER_OK"),
})

RX_RECEPTION = frozenset({
    ("CAM_RX", "RECEPTION_OK"),
    ("DENM", "DENM_RECEPTION_OK"),
})

RX_REPLAY_OK = frozenset({
    ("REPLAY", "REPLAY_OK"),
    ("DENM", "DENM_REPLAY_OK"),
})

RX_REPLAY_DROP = frozenset({
    ("REPLAY", "REPLAY_DROP"),
    ("DENM", "DENM_REPLAY_DROP"),
})
