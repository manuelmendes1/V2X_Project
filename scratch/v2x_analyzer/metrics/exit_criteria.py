"""
Critérios de saída do analyzer alinhados ao checkout atual.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .connectivity import ConnectivityMetrics
from .duplicates import DuplicateMetrics
from .rx_pipeline import RxPipelineMetrics
from .tx_pipeline import TxPipelineMetrics


@dataclass
class Criterion:
    name: str
    passed: bool
    detail: str


@dataclass
class ExitCriteriaResult:
    criteria: list[Criterion] = field(default_factory=list)

    @property
    def all_pass(self) -> bool:
        return all(c.passed for c in self.criteria)


def compute(
    conn: ConnectivityMetrics,
    tx: TxPipelineMetrics,
    rx: RxPipelineMetrics,
    dup: DuplicateMetrics,
) -> ExitCriteriaResult:

    criteria: list[Criterion] = []

    tx_counts = [
        tx.generate_ok,
        tx.encode_ok,
        tx.sign_ok,
        tx.btp_tx_ok,
        tx.geonet_tx_ok,
        tx.access_tx_ok,
        tx.send_ok,
        tx.tx_complete,
    ]

    tx_ok = len(set(tx_counts)) == 1 and tx.tx_complete > 0

    criteria.append(
        Criterion(
            "Cadeia TX completa",
            tx_ok,
            f"counts={tx_counts}",
        )
    )

    rx_counts = [
        rx.raw_rx,
        rx.access_rx_ok,
        rx.geonet_rx_ok,
        rx.btp_rx_ok,
        rx.verify_ok,
        rx.decode_ok,
        rx.replay_ok,
        rx.reception_ok,
        rx.deliver_ok,
    ]

    rx_ok = len(set(rx_counts)) == 1 and rx.deliver_ok > 0

    criteria.append(
        Criterion(
            "Cadeia RX completa",
            rx_ok,
            f"counts={rx_counts}",
        )
    )

    criteria.append(
        Criterion(
            "Sem falhas TX/RX observadas",
            (
                tx.encode_fail == 0
                and tx.sign_fail == 0
                and tx.send_fail == 0
                and rx.verify_fail == 0
                and rx.decode_fail == 0
                and rx.replay_drop == 0
            ),
            (
                f"encode={tx.encode_fail} "
                f"sign={tx.sign_fail} "
                f"send={tx.send_fail} "
                f"verify={rx.verify_fail} "
                f"decode={rx.decode_fail} "
                f"replay={rx.replay_drop}"
            ),
        )
    )

    criteria.append(
        Criterion(
            "Veículos identificados",
            conn.n_vehicles >= 2,
            f"n_vehicles={conn.n_vehicles}",
        )
    )

    criteria.append(
        Criterion(
            "Duplicados",
            dup.duplicated_keys == 0,
            (
                f"duplicated_keys={dup.duplicated_keys} "
                f"duplicate_count={dup.duplicate_count}"
            ),
        )
    )

    return ExitCriteriaResult(criteria=criteria)
