from pathlib import Path

import pytest

from v2x_analyzer.parser import (
    DenmProvisioningEvidence,
    LogEntry,
    load_log,
    parse_line,
)


def test_parse_current_rich_line():
    raw = (
        "[SIM_STEP=16][SIM_TIME=2.800922s][TIMESTAMP=2800922us]"
        "[PKT=8][VPKT=0][SEQ=0][VEH=veh1][NODE=1][STATION=2]"
        "[PIPE_STEP=6/9][STAGE=DECODE][RESULT=OK][REASON=BTP dst_port=2001]"
        "[LVL=DEBUG][MOD=CAM][EV=DECODE_OK] station=1 speed=13.9"
    )

    e = parse_line(raw, 1, "output_log.txt")

    assert e is not None
    assert e.step == 16
    assert e.t == pytest.approx(2.800922)
    assert e.pkt == 8
    assert e.veh_pkt == 0
    assert e.seq == 0
    assert e.veh == "veh1"
    assert e.node == 1
    assert e.station == 2
    assert e.pipe_stage == "DECODE"
    assert e.result == "OK"
    assert e.reason() == "BTP dst_port=2001"
    assert e.module == "CAM"
    assert e.event == "DECODE_OK"


def test_parse_legacy_structure_without_using_legacy_events():
    raw = (
        "[STEP=30][T=4.001642s][LVL=INFO]"
        "[VEH=veh1][NODE=1][STATION=2][SEQ=1]"
        "[MOD=CAM][EV=GENERATE_OK]"
        "reason=DISTANCE"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    assert e.step == 30
    assert e.t == pytest.approx(4.001642)
    assert e.reason() == "DISTANCE"


def test_vpkt_and_veh_pkt_are_structural_aliases():
    raw_vpkt = (
        "[SIM_STEP=1][SIM_TIME=0.1s][PKT=7][VPKT=3][SEQ=1]"
        "[VEH=veh0][NODE=0][STATION=1]"
        "[LVL=DEBUG][MOD=CAM][EV=GENERATE_OK]"
    )

    raw_veh_pkt = (
        "[SIM_STEP=1][SIM_TIME=0.1s][PKT=7][VEH_PKT=3][SEQ=1]"
        "[VEH=veh0][NODE=0][STATION=1]"
        "[LVL=DEBUG][MOD=CAM][EV=GENERATE_OK]"
    )

    a = parse_line(raw_vpkt, 1, "test.txt")
    b = parse_line(raw_veh_pkt, 2, "test.txt")

    assert a is not None
    assert b is not None
    assert a.veh_pkt == 3
    assert b.veh_pkt == 3


def test_duration_from_structured_field():
    raw = (
        "[SIM_STEP=1][SIM_TIME=0.1s][SEQ=1][VEH=veh0]"
        "[NODE=0][STATION=1][DURATION_US=51]"
        "[LVL=DEBUG][MOD=CAM][EV=ENCODE_OK]"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    assert e.duration_us() == 51


def test_duration_from_free_text():
    raw = (
        "[STEP=1][T=0.1s][LVL=DEBUG]"
        "[MOD=SECURITY][EV=SIGN_OK]"
        "[VEH=veh0][NODE=0][STATION=1][SEQ=1]"
        "signature_size=64 DURATION_US=42"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    assert e.duration is None
    assert e.duration_us() == 42


def test_reason_from_free_text():
    raw = (
        "[SIM_STEP=1][SIM_TIME=1s][LVL=INFO]"
        "[MOD=CAM][EV=GENERATE_OK]"
        "[VEH=veh0][NODE=0][STATION=1][SEQ=1]"
        "reason=DISTANCE"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    assert e.reason() == "DISTANCE"


def test_reason_from_structured_field():
    raw = (
        "[SIM_STEP=1][SIM_TIME=0.1s][VEH=veh0][NODE=0]"
        "[STATION=1][SEQ=1][LVL=ERROR][MOD=SECURITY]"
        "[EV=SIGN_FAIL][RESULT=FAIL][REASON=invalid_input]"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    assert e.reason() == "invalid_input"


def test_parse_missing_sim_time():
    raw = (
        "[SIM_STEP=0][VEH=veh0][NODE=0][STATION=1][SEQ=-1]"
        "[LVL=INFO][MOD=VEHICLE][EV=IDENTITY_READY]"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    assert e.t is None


def test_parse_negative_seq():
    raw = (
        "[SIM_STEP=0][VEH=veh0][NODE=0][STATION=1][SEQ=-1]"
        "[LVL=TRACE][MOD=RX][EV=RAW_RX] size=90"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    assert e.seq == -1


def test_parse_vehicle_dash():
    raw = (
        "[SIM_STEP=627][SIM_TIME=80s][VEH=-][NODE=0]"
        "[STATION=0][SEQ=-1][LVL=INFO]"
        "[MOD=RX][EV=RAW_RX]"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    assert e.veh == "-"


def test_kv_extraction():
    raw = (
        "[SIM_STEP=1][SIM_TIME=1s][VEH=veh0][NODE=0]"
        "[STATION=1][SEQ=1][LVL=DEBUG]"
        "[MOD=CAM][EV=GENERATE_OK]"
        "elapsed=1.234567 reason=DISTANCE"
    )

    e = parse_line(raw, 1, "test.txt")

    assert e is not None
    kv = e.kv()
    assert kv["elapsed"] == "1.234567"
    assert kv["reason"] == "DISTANCE"


def test_parse_rejects_unstructured_line():
    raw = "[NET] node=0 ip=10.0.0.1"

    assert parse_line(raw, 1, "test.txt") is None


def test_load_log_counts_unparsed_lines(tmp_path: Path):
    f = tmp_path / "mixed.txt"

    f.write_text(
        "linha de lixo\n"
        "[SIM_STEP=1][SIM_TIME=1s][VEH=veh0][NODE=0][STATION=1]"
        "[SEQ=1][LVL=INFO][MOD=CAM][EV=GENERATE_OK]\n",
        encoding="utf-8",
    )

    entries, stats = load_log([str(f)])

    assert len(entries) == 1
    assert stats.total_lines == 2
    assert stats.parsed_lines == 1
    assert stats.unparsed_lines == 1


def test_load_log_missing_file_exits(tmp_path: Path):
    with pytest.raises(SystemExit):
        load_log([str(tmp_path / "nao_existe.txt")])


def test_load_log_tracks_missing_and_present_sim_time(tmp_path: Path):
    f = tmp_path / "times.txt"

    f.write_text(
        "[SIM_STEP=1][SIM_TIME=1s][VEH=veh0][NODE=0][STATION=1]"
        "[SEQ=1][LVL=INFO][MOD=CAM][EV=GENERATE_OK]\n"
        "[SIM_STEP=2][VEH=veh0][NODE=0][STATION=1]"
        "[SEQ=2][LVL=INFO][MOD=CAM][EV=GENERATE_OK]\n",
        encoding="utf-8",
    )

    entries, stats = load_log([str(f)])

    assert len(entries) == 2
    assert stats.parsed_lines == 2
    assert stats.missing_sim_time == 1
    assert stats.has_sim_time == 1


def test_logentry_reason_field_is_optional():
    e = LogEntry(
        step=1,
        t=1.0,
        level="INFO",
        module="CAM",
        event="GENERATE_OK",
        veh="veh0",
        node=0,
        station=1,
        seq=1,
        pkt=None,
        veh_pkt=None,
        pipe_step=None,
        pipe_stage=None,
        result="OK",
        duration=None,
        payload_type=None,
        payload_size=None,
        msg="reason=FIRST_CAM",
        line_no=1,
        source_file="test",
    )

    assert e.reason_field is None
    assert e.reason() == "FIRST_CAM"


def test_parse_denm_v3_real_signer_provisioning_line():
    from v2x_analyzer.parser import DenmProvisioningEvidence, parse_auxiliary_line

    raw = (
        "[SECURITY] DENM V3 signer=REAL "
        "veh=veh0 "
        "cert=/home/mmendes/vanetza-pki-lab/validation-denm-pki/"
        "veh0-ticket-000.cert"
    )

    evidence = parse_auxiliary_line(raw, 10, "p21_1.log")

    assert isinstance(evidence, DenmProvisioningEvidence)
    assert evidence.vehicle == "veh0"
    assert evidence.cert_path.endswith("veh0-ticket-000.cert")
    assert evidence.role == "signer"


def test_load_log_collects_denm_provisioning_without_creating_logentry(tmp_path):
    log = tmp_path / "p21_provisioning.log"
    log.write_text(
        "[SECURITY] DENM V3 signer=REAL "
        "veh=veh0 cert=/pki/veh0-ticket-000.cert\n"
        "[SIM_STEP=0][SIM_TIME=2.0s][NODE=0][STATION=1][SEQ=1]"
        "[LVL=DEBUG][MOD=SECURITY][EV=SIGN_OK] "
        "DENM_V3 signed protocol_version=3 aid=37 signed=1\n",
        encoding="utf-8",
    )

    entries, stats = load_log([str(log)])

    assert len(entries) == 1
    assert entries[0].event == "SIGN_OK"
    assert stats.denm_provisioning == [
        DenmProvisioningEvidence(
            vehicle="veh0",
            cert_path="/pki/veh0-ticket-000.cert",
            role="signer",
            line_no=1,
            source_file=str(log),
        )
    ]
    assert stats.unparsed_lines == 0
