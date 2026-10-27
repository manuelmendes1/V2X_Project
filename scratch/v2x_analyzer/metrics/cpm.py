"""Metricas CPM baseadas nos eventos concretos do logger."""

from __future__ import annotations

from collections import Counter, defaultdict
from dataclasses import dataclass, field

from ..parser import LogEntry


def _integer(value: str | None) -> int | None:
    if value is None:
        return None
    try:
        number = int(value, 10)
    except (TypeError, ValueError):
        return None
    return number if number >= 0 else None


def _value(entry: LogEntry, key: str) -> int | None:
    return _integer(entry.kv().get(key))


def _source_station(entry: LogEntry) -> int:
    station = _value(entry, "station")
    return entry.station if station is None else station


@dataclass
class CpmMetrics:
    generate_ok: int = 0
    generate_fail: int = 0
    encode_ok: int = 0
    encode_fail: int = 0
    btp_tx_begin: int = 0
    btp_tx_fail: int = 0
    tx_complete: int = 0

    physical_rx_packets: int = 0
    geonet_rx_ok: int = 0
    btp_rx_ok: int = 0
    btp_rx_fail: int = 0
    profile_check_ok: int = 0
    profile_check_fail: int = 0
    security_fail: int = 0
    decode_ok: int = 0
    decode_fail: int = 0
    semantic_validation_fail: int = 0
    sender_identity_fail: int = 0
    self_reception_drop: int = 0
    application_queue_full: int = 0
    replay_ok: int = 0
    replay_drop: int = 0
    reception_ok: int = 0
    deliver_ok: int = 0

    known_objects_total: int = 0
    known_objects_samples: int = 0
    included_objects_total: int = 0
    included_objects_samples: int = 0
    delivered_objects_total: int = 0
    delivered_objects_samples: int = 0

    encoded_size_distribution: dict[int, int] = field(
        default_factory=dict
    )
    encoded_size_samples: int = 0
    encoded_size_max: int | None = None

    generation_interval_samples: int = 0
    generation_interval_min_ms: int | None = None
    generation_interval_mean_ms: float | None = None
    generation_interval_max_ms: int | None = None

    @property
    def known_objects_mean(self) -> float | None:
        if not self.known_objects_samples:
            return None
        return self.known_objects_total / self.known_objects_samples

    def _add_object_sample(
        self,
        entry: LogEntry,
        key: str,
        total_field: str,
        samples_field: str,
    ) -> None:
        count = _value(entry, key)
        if count is None:
            return
        setattr(
            self, total_field,
            getattr(self, total_field) + count,
        )
        setattr(
            self, samples_field,
            getattr(self, samples_field) + 1,
        )


_EVENT_FIELDS = {
    ("CPM", "CPM_GENERATE_OK"): "generate_ok",
    ("CPM", "CPM_GENERATE_FAIL"): "generate_fail",
    ("CPM", "CPM_ENCODE_OK"): "encode_ok",
    ("CPM", "CPM_ENCODE_FAIL"): "encode_fail",
    ("CPM", "CPM_BTP_TX_BEGIN"): "btp_tx_begin",
    ("CPM", "CPM_BTP_TX_FAIL"): "btp_tx_fail",
    ("CPM", "CPM_GEONET_RX_OK"): "geonet_rx_ok",
    ("CPM", "CPM_BTP_RX_OK"): "btp_rx_ok",
    ("CPM", "CPM_BTP_RX_FAIL"): "btp_rx_fail",
    ("CPM", "CPM_PROFILE_CHECK_OK"): "profile_check_ok",
    ("CPM", "CPM_PROFILE_CHECK_FAIL"): "profile_check_fail",
    ("CPM", "CPM_CRYPTO_VERIFY_FAIL"): "security_fail",
    ("CPM", "CPM_DECODE_OK"): "decode_ok",
    ("CPM", "CPM_DECODE_FAIL"): "decode_fail",
    ("CPM", "CPM_SEMANTIC_VALIDATION_FAIL"):
        "semantic_validation_fail",
    ("CPM", "CPM_SENDER_IDENTITY_FAIL"):
        "sender_identity_fail",
    ("CPM", "CPM_SELF_RECEPTION_DROP"): "self_reception_drop",
    ("CPM", "CPM_APPLICATION_QUEUE_FULL"):
        "application_queue_full",
    ("CPM", "CPM_REPLAY_OK"): "replay_ok",
    ("CPM", "CPM_REPLAY_DROP"): "replay_drop",
    ("CPM", "CPM_RECEPTION_OK"): "reception_ok",
    ("CPM", "CPM_DELIVER_OK"): "deliver_ok",
}


def compute(entries: list[LogEntry]) -> CpmMetrics:
    metrics = CpmMetrics()
    sizes: Counter[int] = Counter()
    generation_times: dict[int, list[int]] = defaultdict(list)

    for entry in entries:
        field_name = _EVENT_FIELDS.get(
            (entry.module, entry.event)
        )
        if field_name is not None:
            setattr(
                metrics,
                field_name,
                getattr(metrics, field_name) + 1,
            )

        if (
            entry.module == "TX"
            and entry.event == "TX_COMPLETE"
            and (entry.payload_type or "").upper() == "CPM"
        ):
            metrics.tx_complete += 1

        if (
            entry.module == "CPM"
            and entry.event == "CPM_RX_DISPATCH"
        ):
            metrics.physical_rx_packets += 1

        if (
            entry.module == "CPM"
            and entry.event == "CPM_GENERATE_OK"
        ):
            metrics._add_object_sample(
                entry,
                "known_objects",
                "known_objects_total",
                "known_objects_samples",
            )
            timestamp = _value(entry, "reference_time_its_ms")
            if timestamp is not None:
                generation_times[
                    _source_station(entry)
                ].append(timestamp)

        if (
            entry.module == "CPM"
            and entry.event == "CPM_ENCODE_OK"
        ):
            metrics._add_object_sample(
                entry,
                "included_objects",
                "included_objects_total",
                "included_objects_samples",
            )
            size = _value(entry, "encoded_bytes")
            if size is None and (
                (entry.payload_type or "").upper() == "CPM"
            ):
                size = entry.payload_size
            if size is not None and size > 0:
                sizes[size] += 1

        if (
            entry.module == "CPM"
            and entry.event == "CPM_DELIVER_OK"
        ):
            metrics._add_object_sample(
                entry,
                "included_objects",
                "delivered_objects_total",
                "delivered_objects_samples",
            )

    metrics.encoded_size_distribution = dict(sorted(sizes.items()))
    metrics.encoded_size_samples = sum(sizes.values())
    if sizes:
        metrics.encoded_size_max = max(sizes)

    intervals: list[int] = []
    for timestamps in generation_times.values():
        for earlier, later in zip(
            timestamps, timestamps[1:]
        ):
            if later > earlier:
                intervals.append(later - earlier)

    metrics.generation_interval_samples = len(intervals)
    if intervals:
        metrics.generation_interval_min_ms = min(intervals)
        metrics.generation_interval_mean_ms = (
            sum(intervals) / len(intervals)
        )
        metrics.generation_interval_max_ms = max(intervals)

    return metrics
