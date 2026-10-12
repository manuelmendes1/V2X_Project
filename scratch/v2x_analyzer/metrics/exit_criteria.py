"""
Critérios de saída do analyzer alinhados ao checkout atual.

A pipeline é heterogénea: CAM e DENM partilham as camadas inferiores,
mas não são obrigados a produzir exatamente os mesmos eventos em cada
sub-etapa. Os critérios verificam portanto continuidade monotónica e
consistência entre aceitação/rejeição, em vez de igualdade artificial
entre todos os contadores.
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


def _monotonic_non_increasing(values: list[int]) -> bool:
    return all(a >= b for a, b in zip(values, values[1:]))


def compute(
    conn: ConnectivityMetrics,
    tx: TxPipelineMetrics,
    rx: RxPipelineMetrics,
    dup: DuplicateMetrics,
) -> ExitCriteriaResult:

    criteria: list[Criterion] = []

    # SIGN is protocol-dependent. It is validated when present, but it
    # is not a mandatory link in the common transport chain. SPATEM's
    # current transport path does not emit cryptographic SIGN events.
    tx_counts = [
        tx.generate_ok,
        tx.encode_ok,
        tx.btp_tx_ok,
        tx.geonet_tx_ok,
        tx.access_tx_ok,
        tx.send_ok,
        tx.tx_complete,
    ]

    sign_consistent = (
        tx.sign_begin == 0
        or tx.sign_begin <= tx.sign_ok
    )

    tx_ok = (
        tx.tx_complete > 0
        and _monotonic_non_increasing(tx_counts)
        and tx.encode_begin <= tx.encode_ok
        and sign_consistent
        and tx.access_tx_begin >= tx.access_tx_ok
        and tx.send_begin >= tx.send_ok
    )

    criteria.append(
        Criterion(
            "Cadeia TX consistente",
            tx_ok,
            f"counts={tx_counts}",
        )
    )

    rx_stage_counts = [
        rx.raw_rx,
        rx.access_rx_ok,
        rx.geonet_rx_ok,
        rx.btp_rx_ok,
        rx.verify_ok,
        rx.decode_ok,
    ]

    replay_accounted = (
        rx.replay_ok + rx.replay_drop == rx.decode_ok
    )

    rx_ok = (
        rx.deliver_ok > 0
        and _monotonic_non_increasing(rx_stage_counts)
        and replay_accounted
        and rx.replay_ok == rx.reception_ok
        and rx.reception_ok == rx.deliver_ok
    )

    criteria.append(
        Criterion(
            "Cadeia RX consistente",
            rx_ok,
            (
                f"stages={rx_stage_counts} "
                f"replay_ok={rx.replay_ok} "
                f"replay_drop={rx.replay_drop} "
                f"replay_accounted={replay_accounted} "
                f"reception={rx.reception_ok} "
                f"deliver={rx.deliver_ok}"
            ),
        )
    )

    # Replay drops are an expected security/filtering outcome, not an
    # operational TX/RX failure. Actual failure events remain hard failures.
    criteria.append(
        Criterion(
            "Sem falhas TX/RX observadas",
            (
                tx.encode_fail == 0
                and tx.sign_fail == 0
                and tx.send_fail == 0
                and rx.verify_fail == 0
                and rx.decode_fail == 0
            ),
            (
                f"encode={tx.encode_fail} "
                f"sign={tx.sign_fail} "
                f"send={tx.send_fail} "
                f"verify={rx.verify_fail} "
                f"decode={rx.decode_fail} "
                f"replay_drop_filtered={rx.replay_drop}"
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
