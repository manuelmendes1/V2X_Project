"""
Parser dos logs V2X.

O parser aceita as duas formas estruturais de logging observadas no projeto:

1. Atual:
   [SIM_STEP=N][SIM_TIME=Xs]...[LVL=...][MOD=...][EV=...]

2. Legada:
   [STEP=N][T=Xs]...[LVL=...][MOD=...][EV=...]

A compatibilidade aqui é estrutural, não semântica:
o analyzer de métricas continua a reconhecer exclusivamente os eventos
da taxonomia operacional atual.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field


# ---------------------------------------------------------------------------
# Regexes
# ---------------------------------------------------------------------------

_TAG_RE = re.compile(
    r"\[([A-Za-z0-9_]+)=([^\]]*)\]"
)

_KV_RE = re.compile(
    r"(?<![A-Za-z0-9_])"
    r"([A-Za-z_][A-Za-z0-9_]*)"
    r"="
    r"([^\s]+)"
)

_NUMBER_RE = re.compile(
    r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)"
)

_POS_RE = re.compile(
    r"pos=\("
    r"(?P<x>-?(?:\d+(?:\.\d*)?|\.\d+))"
    r","
    r"(?P<y>-?(?:\d+(?:\.\d*)?|\.\d+))"
    r"\)"
)

_POS_XY_RE = re.compile(
    r"pos_x=(-?(?:\d+(?:\.\d*)?|\.\d+))"
    r"\s+"
    r"pos_y=(-?(?:\d+(?:\.\d*)?|\.\d+))"
)

_DURATION_FREE_RE = re.compile(
    r"\bDURATION_US\s*=\s*(-?\d+)\b"
)

_REASON_FREE_RE = re.compile(
    r"\breason\s*=\s*([^\s]+)",
    re.IGNORECASE,
)

_DENM_REAL_SIGNER_RE = re.compile(
    r"^\[SECURITY\]\s+DENM\s+V3\s+"
    r"signer=REAL\s+"
    r"veh=(?P<vehicle>[^\s]+)\s+"
    r"cert=(?P<cert_path>\S+)\s*$"
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _to_int(value: str | None) -> int | None:
    if value is None or value == "":
        return None

    try:
        return int(value)
    except ValueError:
        return None


def _to_float(value: str | None) -> float | None:
    if value is None or value == "":
        return None

    match = _NUMBER_RE.search(value)
    if not match:
        return None

    try:
        return float(match.group(0))
    except ValueError:
        return None


def _parse_tags(raw: str) -> tuple[dict[str, str], int]:
    """
    Extrai todos os [KEY=VALUE], independentemente da ordem.

    Devolve:
        tags, posição final do último campo estruturado
    """

    tags: dict[str, str] = {}
    last_end = 0

    for match in _TAG_RE.finditer(raw):
        key = match.group(1)
        value = match.group(2)

        tags[key] = value
        last_end = match.end()

    return tags, last_end


# ---------------------------------------------------------------------------
# Evidência auxiliar de provisioning DENM V3
# ---------------------------------------------------------------------------

@dataclass(frozen=True, slots=True)
class DenmProvisioningEvidence:
    vehicle: str
    cert_path: str
    role: str
    line_no: int
    source_file: str


def parse_auxiliary_line(
    raw: str,
    line_no: int,
    source_file: str,
) -> DenmProvisioningEvidence | None:
    raw = raw.rstrip("\n\r")

    match = _DENM_REAL_SIGNER_RE.match(raw)
    if not match:
        return None

    return DenmProvisioningEvidence(
        vehicle=match.group("vehicle"),
        cert_path=match.group("cert_path"),
        role="signer",
        line_no=line_no,
        source_file=source_file,
    )


# ---------------------------------------------------------------------------
# Entrada estruturada
# ---------------------------------------------------------------------------

@dataclass(frozen=True, slots=True)
class LogEntry:
    step: int
    t: float | None

    level: str
    module: str
    event: str

    veh: str
    node: int
    station: int
    seq: int

    pkt: int | None
    veh_pkt: int | None

    pipe_step: str | None
    pipe_stage: str | None

    result: str | None

    duration: int | None

    payload_type: str | None
    payload_size: int | None

    msg: str

    line_no: int
    source_file: str

    # Opcional para permitir construção programática de LogEntry por
    # consumidores antigos do parser.
    reason_field: str | None = None

    # ---------------------------------------------------------------------

    def kv(self) -> dict[str, str]:
        """
        Extrai key=value tanto da mensagem livre como é feito no logger
        atual. Campos estruturados entre [] não fazem parte de msg.
        """
        return dict(_KV_RE.findall(self.msg))

    # ---------------------------------------------------------------------

    def reason(self) -> str | None:
        """
        Obtém REASON do campo estruturado ou do texto livre.
        """

        if self.reason_field:
            return self.reason_field

        m = _REASON_FREE_RE.search(self.msg)
        if m:
            return m.group(1)

        return None

    # ---------------------------------------------------------------------

    def pos(self) -> tuple[float, float] | None:

        m = _POS_XY_RE.search(self.msg)
        if m:
            return (
                float(m.group(1)),
                float(m.group(2)),
            )

        m = _POS_RE.search(self.msg)
        if m:
            return (
                float(m.group("x")),
                float(m.group("y")),
            )

        return None

    # ---------------------------------------------------------------------

    def duration_us(self) -> int | None:
        """
        Campo estruturado [DURATION_US=N] quando presente.

        Fallback para DURATION_US=N escrito no texto livre, que é usado
        pela API legacy do logger.
        """

        if self.duration is not None:
            return self.duration

        match = _DURATION_FREE_RE.search(self.msg)
        if match:
            return int(match.group(1))

        return None

    # ---------------------------------------------------------------------

    def is_(self, module: str, event: str) -> bool:
        return (
            self.module == module
            and self.event == event
        )

    # ---------------------------------------------------------------------

    def is_any(
        self,
        aliases: frozenset[tuple[str, str]],
    ) -> bool:
        return (
            self.module,
            self.event,
        ) in aliases


# ---------------------------------------------------------------------------
# Estatísticas do parser
# ---------------------------------------------------------------------------

@dataclass
class ParseStats:
    files: list[str] = field(default_factory=list)

    total_lines: int = 0
    parsed_lines: int = 0
    unparsed_lines: int = 0

    unparsed_samples: list[str] = field(default_factory=list)

    denm_provisioning: list[DenmProvisioningEvidence] = field(
        default_factory=list
    )

    missing_sim_time: int = 0

    @property
    def has_sim_time(self) -> int:
        return max(self.parsed_lines - self.missing_sim_time, 0)

    @property
    def unparsed_ratio(self) -> float:
        if self.total_lines == 0:
            return 0.0

        return self.unparsed_lines / self.total_lines


# ---------------------------------------------------------------------------
# Parse individual
# ---------------------------------------------------------------------------

def parse_line(
    raw: str,
    line_no: int,
    source_file: str,
) -> LogEntry | None:

    raw = raw.rstrip("\n\r")

    if not raw:
        return None

    tags, last_end = _parse_tags(raw)

    # O evento estruturado mínimo necessário.
    module = tags.get("MOD")
    event = tags.get("EV")

    if not module or not event:
        return None

    # ------------------------------------------------------------------
    # STEP / SIM_STEP
    # ------------------------------------------------------------------

    step = _to_int(
        tags.get("SIM_STEP")
        if tags.get("SIM_STEP") is not None
        else tags.get("STEP")
    )

    # ------------------------------------------------------------------
    # SIM_TIME / T
    # ------------------------------------------------------------------

    time_raw = (
        tags.get("SIM_TIME")
        if tags.get("SIM_TIME") is not None
        else tags.get("T")
    )

    t = _to_float(time_raw)

    # ------------------------------------------------------------------
    # Identity básica
    # ------------------------------------------------------------------

    veh = tags.get("VEH", "")

    node = _to_int(tags.get("NODE"))
    station = _to_int(tags.get("STATION"))
    seq = _to_int(tags.get("SEQ"))

    if step is None or node is None or station is None or seq is None:
        return None

    # ------------------------------------------------------------------
    # Packet identity
    # ------------------------------------------------------------------

    pkt = _to_int(tags.get("PKT"))

    # Atual = VPKT
    # Variante anterior = VEH_PKT
    veh_pkt_raw = (
        tags.get("VPKT")
        if tags.get("VPKT") is not None
        else tags.get("VEH_PKT")
    )

    veh_pkt = _to_int(veh_pkt_raw)

    # ------------------------------------------------------------------
    # Pipeline fields
    # ------------------------------------------------------------------

    pipe_step = tags.get("PIPE_STEP")

    pipe_stage = (
        tags.get("STAGE")
        if tags.get("STAGE") is not None
        else tags.get("PIPE_STAGE")
    )

    result = tags.get("RESULT")
    reason_field = tags.get("REASON")

    duration = _to_int(tags.get("DURATION_US"))

    payload_type = tags.get("PAYLOAD_TYPE")
    payload_size = _to_int(tags.get("PAYLOAD_SIZE"))

    # ------------------------------------------------------------------
    # Mensagem livre
    # ------------------------------------------------------------------

    msg = raw[last_end:]

    if msg.startswith(" "):
        msg = msg[1:]

    return LogEntry(
        step=step,
        t=t,

        level=tags.get("LVL", ""),
        module=module,
        event=event,

        veh=veh,
        node=node,
        station=station,
        seq=seq,

        pkt=pkt,
        veh_pkt=veh_pkt,

        pipe_step=pipe_step,
        pipe_stage=pipe_stage,

        result=result,
        reason_field=reason_field,

        duration=duration,

        payload_type=payload_type,
        payload_size=payload_size,

        msg=msg,

        line_no=line_no,
        source_file=source_file,
    )


# ---------------------------------------------------------------------------
# Load completo
# ---------------------------------------------------------------------------

def load_log(
    files: list[str],
) -> tuple[list[LogEntry], ParseStats]:

    entries: list[LogEntry] = []

    stats = ParseStats(
        files=list(files),
    )

    for source_file in files:
        try:
            handle = open(
                source_file,
                "r",
                encoding="utf-8",
                errors="replace",
            )
        except OSError as exc:
            raise SystemExit(
                f"Não foi possível abrir o ficheiro: {source_file}: {exc}"
            ) from exc

        with handle:

            for line_no, raw in enumerate(handle, start=1):

                stats.total_lines += 1

                entry = parse_line(
                    raw,
                    line_no,
                    source_file,
                )

                if entry is None:
                    auxiliary = parse_auxiliary_line(
                        raw,
                        line_no,
                        source_file,
                    )

                    if auxiliary is not None:
                        stats.denm_provisioning.append(auxiliary)
                        continue

                    stats.unparsed_lines += 1

                    if len(stats.unparsed_samples) < 5:
                        sample = raw.rstrip("\n\r")

                        if sample:
                            stats.unparsed_samples.append(sample)

                    continue

                stats.parsed_lines += 1

                if entry.t is None:
                    stats.missing_sim_time += 1

                entries.append(entry)

    return entries, stats
