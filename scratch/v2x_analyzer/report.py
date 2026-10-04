"""
Relatório do v2x_analyzer para a taxonomia operacional atual.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, is_dataclass
from typing import Any

from .parser import LogEntry, ParseStats
from .identity import IdentityMap, build_identity_map
from .metrics import (
    connectivity,
    denm,
    duplicates,
    errors,
    exit_criteria,
    rx_pipeline,
    timing,
    trigger_reasons,
    tx_pipeline,
    vehicles,
)


@dataclass
class FullReport:
    parse_stats: ParseStats
    identity: IdentityMap
    denm: denm.DenmReport
    tx: tx_pipeline.TxPipelineMetrics
    triggers: trigger_reasons.TriggerReasonMetrics
    rx: rx_pipeline.RxPipelineMetrics
    err: errors.ErrorMetrics
    conn: connectivity.ConnectivityMetrics
    veh: vehicles.VehicleMetrics
    time_: timing.TimingMetrics
    dup: duplicates.DuplicateMetrics
    exit_: exit_criteria.ExitCriteriaResult


def build_report(
    entries: list[LogEntry],
    stats: ParseStats,
) -> FullReport:

    identity = build_identity_map(entries)
    denm_report = denm.DenmReport(
        lifecycle=denm.compute_lifecycle(entries),
        credentials=denm.compute_credentials(
            entries,
            getattr(stats, "denm_provisioning", ()),
        ),
    )
    tx = tx_pipeline.compute(entries)
    triggers = trigger_reasons.compute(entries)
    rx = rx_pipeline.compute(entries)
    err = errors.compute(entries)
    conn = connectivity.compute(
        entries,
        identity,
        tx_complete=tx.tx_complete,
    )
    veh = vehicles.compute(entries, identity)
    time_ = timing.compute(entries)
    dup = duplicates.compute(entries, identity)
    exit_ = exit_criteria.compute(conn, tx, rx, dup)

    return FullReport(
        parse_stats=stats,
        identity=identity,
        denm=denm_report,
        tx=tx,
        triggers=triggers,
        rx=rx,
        err=err,
        conn=conn,
        veh=veh,
        time_=time_,
        dup=dup,
        exit_=exit_,
    )


def _pct(n: int, d: int) -> str:
    if d == 0:
        return "N/A"
    return f"{100.0 * n / d:.2f}%"


def render_text(r: FullReport) -> str:
    lines: list[str] = []
    w = lines.append

    total = r.parse_stats.total_lines
    ratio = (
        r.parse_stats.unparsed_lines / total
        if total
        else 0.0
    )

    w("=" * 72)
    w("     V2X PIPELINE AUDIT REPORT — CURRENT TAXONOMY")
    w("=" * 72)
    w("")
    w(f"Ficheiros: {', '.join(r.parse_stats.files)}")
    w(
        f"Linhas lidas: {total}  "
        f"parseadas: {r.parse_stats.parsed_lines}  "
        f"não reconhecidas: {r.parse_stats.unparsed_lines} "
        f"({ratio * 100:.2f}%)"
    )

    if r.parse_stats.unparsed_samples:
        w("Amostras de linhas não reconhecidas:")
        for sample in r.parse_stats.unparsed_samples:
            w(f"  {sample}")

    w("")
    w("-" * 72)
    w("PIPELINE TX")
    w("-" * 72)

    tx_rows = [
        ("Generate OK", r.tx.generate_ok),
        ("Encode Begin", r.tx.encode_begin),
        ("Encode OK", r.tx.encode_ok),
        ("Encode Fail", r.tx.encode_fail),
        ("Sign Begin", r.tx.sign_begin),
        ("Sign OK", r.tx.sign_ok),
        ("Sign Fail", r.tx.sign_fail),
        ("BTP TX Begin", r.tx.btp_tx_begin),
        ("BTP TX OK", r.tx.btp_tx_ok),
        ("GeoNet TX Begin", r.tx.geonet_tx_begin),
        ("GeoNet TX OK", r.tx.geonet_tx_ok),
        ("Access TX Begin", r.tx.access_tx_begin),
        ("Access TX OK", r.tx.access_tx_ok),
        ("Send Begin", r.tx.send_begin),
        ("Send OK", r.tx.send_ok),
        ("Send Fail", r.tx.send_fail),
        ("TX Complete", r.tx.tx_complete),
    ]

    for label, value in tx_rows:
        w(f"  {label:<24}{value:>7}")

    w("")
    w("-" * 72)
    w("MOTIVOS DE GERAÇÃO")
    w("-" * 72)
    w(f"  {'Distance':<24}{r.triggers.distance:>7}")
    w(f"  {'Speed':<24}{r.triggers.speed:>7}")
    w(f"  {'Heading':<24}{r.triggers.heading:>7}")
    w(f"  {'Max Time':<24}{r.triggers.maxtime:>7}")
    w(f"  {'First CAM':<24}{r.triggers.first_cam:>7}")
    w(f"  {'Total':<24}{r.triggers.total:>7}")

    w("")
    w("-" * 72)
    w("PIPELINE RX")
    w("-" * 72)

    rx_rows = [
        ("Raw RX", r.rx.raw_rx),
        ("Access RX Begin", r.rx.access_rx_begin),
        ("Access RX OK", r.rx.access_rx_ok),
        ("GeoNet RX Begin", r.rx.geonet_rx_begin),
        ("GeoNet RX OK", r.rx.geonet_rx_ok),
        ("BTP RX Begin", r.rx.btp_rx_begin),
        ("BTP RX OK", r.rx.btp_rx_ok),
        ("Verify Begin", r.rx.verify_begin),
        ("Verify OK", r.rx.verify_ok),
        ("Verify Fail", r.rx.verify_fail),
        ("Decode OK", r.rx.decode_ok),
        ("Decode Fail", r.rx.decode_fail),
        ("Replay OK", r.rx.replay_ok),
        ("Replay Drop", r.rx.replay_drop),
        ("Reception Begin", r.rx.reception_begin),
        ("Reception OK", r.rx.reception_ok),
        ("Deliver Begin", r.rx.deliver_begin),
        ("Deliver OK", r.rx.deliver_ok),
    ]

    for label, value in rx_rows:
        w(f"  {label:<24}{value:>7}")

    w("")
    w("-" * 72)
    w("DENM")
    w("-" * 72)
    w(f"  {'NEW':<24}{r.denm.lifecycle.new:>7}")
    w(f"  {'UPDATE':<24}{r.denm.lifecycle.update:>7}")
    w(f"  {'CANCELLATION':<24}{r.denm.lifecycle.cancellation:>7}")
    w(f"  {'Logical Events':<24}{len(r.denm.lifecycle.logical_events):>7}")
    w(
        f"  {'DENM V3 Signatures':<24}"
        f"{r.denm.credentials.denm_v3_signatures:>7}"
    )
    w(
        f"  {'DENM V3 AIDs':<24}"
        f"{', '.join(str(aid) for aid in sorted(r.denm.credentials.denm_v3_aids)) or 'none'}"
    )
    w(
        f"  {'DENM V3 Signers':<24}"
        f"{', '.join(sorted(r.denm.credentials.signers)) or 'none'}"
    )
    w(
        f"  {'DENM V3 Real Signers':<24}"
        f"{', '.join(sorted(r.denm.credentials.real_signers)) or 'none'}"
    )
    w("  DENM V3 Certificate Paths:")
    for vehicle, cert_path in sorted(
        r.denm.credentials.certificate_paths.items()
    ):
        w(f"    {vehicle}={cert_path}")

    w("")
    w("-" * 72)
    w("ERROS")
    w("-" * 72)
    w(f"  {'Encode Fail':<24}{r.err.encode_fail:>7}")
    w(f"  {'Sign Fail':<24}{r.err.sign_fail:>7}")
    w(f"  {'Send Fail':<24}{r.err.send_fail:>7}")
    w(f"  {'Verify Fail':<24}{r.err.verify_fail:>7}")
    w(f"  {'Decode Fail':<24}{r.err.decode_fail:>7}")
    w(f"  {'Replay Drop':<24}{r.err.replay_drop:>7}")
    w(f"  {'Total':<24}{r.err.total:>7}")

    w("")
    w("-" * 72)
    w("CONECTIVIDADE")
    w("-" * 72)
    w(f"  Veículos: {r.conn.n_vehicles}")
    w(f"  Pares ativos: {r.conn.n_active_pairs}/{r.conn.max_pairs}")
    w(f"  TX Complete: {r.conn.tx_complete}")
    w(f"  Deliver OK: {r.conn.deliver_ok}")

    delivery = r.conn.effective_delivery_pct
    fanout = r.conn.avg_fanout

    w(
        f"  Entrega efetiva: "
        f"{delivery:.2f}%"
        if delivery is not None
        else "  Entrega efetiva: N/A"
    )
    w(
        f"  Fan-out médio: "
        f"{fanout:.2f}"
        if fanout is not None
        else "  Fan-out médio: N/A"
    )

    w("")
    w("-" * 72)
    w("POR VEÍCULO")
    w("-" * 72)

    w("  TX Complete:")
    for veh, count in r.veh.tx_by_vehicle.most_common():
        w(f"    {veh:<12}{count:>7}")

    w("  Receções:")
    for veh, count in r.veh.rx_by_vehicle.most_common():
        w(f"    {veh:<12}{count:>7}")

    ghosts = r.veh.ghost_senders()
    w(
        "  Ghost check: limpo"
        if not ghosts
        else f"  Ghost check: {', '.join(ghosts)}"
    )

    w("")
    w("-" * 72)
    w("DUPLICADOS")
    w("-" * 72)
    w(f"  Receções com packetId válido: {r.dup.total_valid_receptions}")
    w(f"  Chaves duplicadas: {r.dup.duplicated_keys}")
    w(f"  Receções duplicadas: {r.dup.duplicate_count}")
    w(f"  packetId inválido ignorado: {r.dup.skipped_invalid_pkt}")
    w(f"  Emissor não resolvido: {r.dup.skipped_unresolved_sender}")

    w("")
    w("-" * 72)
    w("DURAÇÕES (us)")
    w("-" * 72)

    for label, op in r.time_.operations.items():
        if op.n == 0:
            w(f"  {label:<12} sem amostras")
            continue

        w(
            f"  {label:<12}"
            f"n={op.n:<5} "
            f"min={op.min_us} "
            f"mediana={op.median_us:.1f} "
            f"media={op.mean_us:.1f} "
            f"p95={op.p95_us:.1f} "
            f"max={op.max_us}"
        )

    w("")
    w("-" * 72)
    w("CRITÉRIOS DE SAÍDA")
    w("-" * 72)

    for criterion in r.exit_.criteria:
        status = "PASS" if criterion.passed else "FAIL"
        w(
            f"  [{status}] "
            f"{criterion.name}: {criterion.detail}"
        )

    verdict = "PASS" if r.exit_.all_pass else "FAIL"
    w("")
    w(f"RESULTADO P3: {verdict}")
    w("")
    w("=" * 72)

    return "\n".join(lines)


def _to_jsonable(obj: Any) -> Any:
    if is_dataclass(obj) and not isinstance(obj, type):
        return {
            k: _to_jsonable(v)
            for k, v in asdict(obj).items()
        }

    if isinstance(obj, set):
        return sorted(obj)

    if isinstance(obj, dict):
        return {
            str(k): _to_jsonable(v)
            for k, v in obj.items()
        }

    if isinstance(obj, (list, tuple)):
        return [_to_jsonable(v) for v in obj]

    return obj


def render_json(r: FullReport) -> str:
    data = _to_jsonable(r)

    data["conn"]["n_active_pairs"] = r.conn.n_active_pairs
    data["conn"]["theoretical_max_receptions"] = (
        r.conn.theoretical_max_receptions
    )
    data["conn"]["effective_delivery_pct"] = (
        r.conn.effective_delivery_pct
    )
    data["conn"]["avg_fanout"] = r.conn.avg_fanout

    data["veh"]["ghost_senders"] = r.veh.ghost_senders()

    data["dup"]["top_duplicates"] = [
        [list(key), count]
        for key, count in r.dup.top_duplicates()
    ]

    data["exit_criteria_all_pass"] = r.exit_.all_pass

    return json.dumps(
        data,
        indent=2,
        ensure_ascii=False,
    )
