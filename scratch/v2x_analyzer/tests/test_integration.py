from v2x_analyzer.parser import LogEntry
from v2x_analyzer.report import build_report


def mk(
    module,
    event,
    veh="veh0",
    node=0,
    station=1,
    seq=1,
    msg="",
    step=1,
    t=1.0,
    duration=None,
):
    return LogEntry(
        step=step,
        t=t,
        level="INFO",
        module=module,
        event=event,
        veh=veh,
        node=node,
        station=station,
        seq=seq,
        pkt=None,
        veh_pkt=None,
        pipe_step=None,
        pipe_stage=None,
        result="OK",
        duration=duration,
        payload_type=None,
        payload_size=None,
        msg=msg,
        line_no=1,
        source_file="integration",
    )


def test_current_pipeline_end_to_end():
    entries = [
        mk("VEHICLE", "IDENTITY_READY", "veh0", 0, 1),
        mk("VEHICLE", "IDENTITY_READY", "veh1", 1, 2),

        mk("CAM", "GENERATE_OK", msg="reason=FIRST_CAM"),
        mk("CAM", "ENCODE_BEGIN"),
        mk("CAM", "ENCODE_OK", duration=20),
        mk("SECURITY", "SIGN_BEGIN"),
        mk("SECURITY", "SIGN_OK", duration=30),
        mk("BTP", "BTP_TX_BEGIN"),
        mk("BTP", "BTP_TX_OK"),
        mk("GEONET", "GEONET_TX_BEGIN"),
        mk("GEONET", "GEONET_TX_OK"),
        mk("ACCESS", "ACCESS_TX_BEGIN"),
        mk("ACCESS", "ACCESS_TX_OK"),
        mk("ACCESS", "SEND_BEGIN"),
        mk("ACCESS", "SEND_OK"),
        mk("TX", "TX_COMPLETE"),

        mk("RX", "RAW_RX", "veh1", 1, 2),
        mk("ACCESS", "ACCESS_RX_BEGIN", "veh1", 1, 2),
        mk("ACCESS", "ACCESS_RX_OK", "veh1", 1, 2),
        mk("GEONET", "GEONET_RX_BEGIN", "veh1", 1, 2),
        mk("GEONET", "GEONET_RX_OK", "veh1", 1, 2),
        mk("BTP", "BTP_RX_BEGIN", "veh1", 1, 2),
        mk("BTP", "BTP_RX_OK", "veh1", 1, 2),
        mk("SECURITY", "VERIFY_BEGIN", "veh1", 1, 2),
        mk("SECURITY", "VERIFY_OK", "veh1", 1, 2),
        mk("CAM", "DECODE_OK", "veh1", 1, 2),
        mk("REPLAY", "REPLAY_OK", "veh1", 1, 2),
        mk(
            "CAM_RX",
            "RECEPTION_OK",
            "veh1",
            1,
            2,
            1,
            (
                "sender=veh0 sender_node=0 sender_station=1 "
                "receiver=veh1 receiver_node=1 receiver_station=2 "
                "pkt=8 vpkt=1 seq=1"
            ),
        ),
        mk(
            "RX",
            "DELIVER_OK",
            "veh1",
            1,
            2,
            1,
            "sender=veh0 sender_station=1 pkt=8 vpkt=1 seq=1",
        ),
    ]

    class Stats:
        files = ["integration"]
        total_lines = len(entries)
        parsed_lines = len(entries)
        unparsed_lines = 0
        unparsed_samples = []

    report = build_report(entries, Stats())

    assert report.tx.tx_complete == 1
    assert report.rx.deliver_ok == 1
    assert report.conn.active_pairs == {("veh0", "veh1")}
    assert report.conn.n_vehicles == 2
