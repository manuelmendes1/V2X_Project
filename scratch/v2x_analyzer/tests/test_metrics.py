from v2x_analyzer.parser import LogEntry
from v2x_analyzer.identity import build_identity_map
from v2x_analyzer.metrics import (
    connectivity,
    duplicates,
    errors,
    rx_pipeline,
    timing,
    trigger_reasons,
    tx_pipeline,
    vehicles,
)


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
    level="INFO",
    pkt=None,
    veh_pkt=None,
    pipe_step=None,
    pipe_stage=None,
    result=None,
    duration=None,
):
    return LogEntry(
        step=step,
        t=t,
        level=level,
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
        duration=duration,
        payload_type=None,
        payload_size=None,
        msg=msg,
        line_no=1,
        source_file="test",
    )


def current_identity():
    return build_identity_map([
        mk(
            "VEHICLE",
            "IDENTITY_READY",
            veh="veh0",
            node=0,
            station=1,
        ),
        mk(
            "VEHICLE",
            "IDENTITY_READY",
            veh="veh1",
            node=1,
            station=2,
        ),
    ])


def current_tx():
    return [
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
    ]


def current_rx():
    return [
        mk("RX", "RAW_RX", veh="veh1", node=1, station=2),
        mk("ACCESS", "ACCESS_RX_BEGIN", veh="veh1", node=1, station=2),
        mk("ACCESS", "ACCESS_RX_OK", veh="veh1", node=1, station=2),
        mk("GEONET", "GEONET_RX_BEGIN", veh="veh1", node=1, station=2),
        mk("GEONET", "GEONET_RX_OK", veh="veh1", node=1, station=2),
        mk("BTP", "BTP_RX_BEGIN", veh="veh1", node=1, station=2),
        mk("BTP", "BTP_RX_OK", veh="veh1", node=1, station=2),
        mk("SECURITY", "VERIFY_BEGIN", veh="veh1", node=1, station=2),
        mk("SECURITY", "VERIFY_OK", veh="veh1", node=1, station=2),
        mk("CAM", "DECODE_OK",
           veh="veh1", node=1, station=2),
        mk("REPLAY", "REPLAY_OK",
           veh="veh1", node=1, station=2),
        mk(
            "CAM_RX",
            "RECEPTION_OK",
            veh="veh1",
            node=1,
            station=2,
            seq=1,
            msg=(
                "sender=veh0 sender_node=0 sender_station=1 "
                "receiver=veh1 receiver_node=1 receiver_station=2 "
                "pkt=8 vpkt=1 seq=1"
            ),
        ),
        mk(
            "RX",
            "DELIVER_OK",
            veh="veh1",
            node=1,
            station=2,
            seq=1,
            msg=(
                "sender=veh0 sender_station=1 "
                "pkt=8 vpkt=1 seq=1"
            ),
        ),
    ]


def test_current_tx_taxonomy():
    m = tx_pipeline.compute(current_tx())

    assert m.generate_ok == 1
    assert m.encode_begin == 1
    assert m.encode_ok == 1
    assert m.sign_ok == 1
    assert m.btp_tx_ok == 1
    assert m.geonet_tx_ok == 1
    assert m.access_tx_ok == 1
    assert m.send_ok == 1
    assert m.tx_complete == 1


def test_current_rx_taxonomy():
    m = rx_pipeline.compute(current_rx())

    assert m.raw_rx == 1
    assert m.access_rx_ok == 1
    assert m.geonet_rx_ok == 1
    assert m.btp_rx_ok == 1
    assert m.verify_ok == 1
    assert m.decode_ok == 1
    assert m.replay_ok == 1
    assert m.reception_ok == 1
    assert m.deliver_ok == 1


def test_identity_uses_current_identity_ready():
    identity = current_identity()

    assert identity.n_vehicles == 2
    assert identity.veh_to_node["veh0"] == 0
    assert identity.veh_to_station["veh1"] == 2


def test_connectivity_uses_deliver_ok():
    from v2x_analyzer.metrics import connectivity

    tx = current_tx()
    rx = current_rx()

    entries = [
        mk("VEHICLE", "IDENTITY_READY",
           veh="veh0", node=0, station=1),
        mk("VEHICLE", "IDENTITY_READY",
           veh="veh1", node=1, station=2),
    ] + tx + rx
    identity = current_identity()

    tx_metrics = tx_pipeline.compute(entries)
    conn = connectivity.compute(
        entries,
        identity,
        tx_complete=tx_metrics.tx_complete,
    )

    assert conn.tx_complete == 1
    assert conn.deliver_ok == 1
    assert conn.active_pairs == {("veh0", "veh1")}


def test_vehicle_metrics_use_tx_complete_and_deliver():
    entries = current_tx() + current_rx()
    identity = current_identity()

    metrics = vehicles.compute(entries, identity)

    assert metrics.tx_by_vehicle["veh0"] == 1
    assert metrics.rx_by_vehicle["veh1"] == 1
    assert metrics.tx_success_by_vehicle["veh0"] == 1


def test_trigger_reason_comes_from_generate_ok():
    m = trigger_reasons.compute(current_tx())

    assert m.first_cam == 1
    assert m.total == 1


def test_timing_uses_current_decode_module():
    entries = current_tx() + current_rx()

    entries.append(
        mk(
            "CAM",
            "DECODE_OK",
            veh="veh1",
            node=1,
            station=2,
            duration=40,
        )
    )

    m = timing.compute(entries)

    assert m.operations["encode"].n == 1
    assert m.operations["sign"].n == 1
    assert m.operations["decode"].n == 1
    assert m.operations["decode"].samples == [40]


def test_denm_tx_events_follow_current_tx_taxonomy():
    entries = [
        mk(
            "DENM",
            "GENERATE_OK",
            msg="reason=BASELINE_NEW",
            veh="veh0",
            node=0,
            station=1,
        ),
        mk(
            "DENM",
            "ENCODE_OK",
            msg="bytes=43",
            veh="veh0",
            node=0,
            station=1,
        ),
        mk(
            "SECURITY",
            "SIGN_BEGIN",
            msg="DENM_V3 signing",
            veh="veh0",
            node=0,
            station=1,
        ),
        mk(
            "SECURITY",
            "SIGN_OK",
            msg="DENM_V3 signed protocol_version=3 aid=37 signed=1",
            veh="veh0",
            node=0,
            station=1,
        ),
    ]

    m = tx_pipeline.compute(entries)

    assert m.generate_ok == 1
    assert m.encode_ok == 1
    assert m.sign_begin == 1
    assert m.sign_ok == 1


def test_denm_rx_events_follow_current_rx_taxonomy():
    entries = [
        mk(
            "DENM",
            "DENM_DECODE_OK",
            veh="veh1",
            node=1,
            station=2,
        ),
        mk(
            "DENM",
            "DENM_REPLAY_OK",
            veh="veh1",
            node=1,
            station=2,
        ),
        mk(
            "DENM",
            "DENM_RECEPTION_OK",
            veh="veh1",
            node=1,
            station=2,
            msg=(
                "sender=veh0 sender_station=1 "
                "receiver=veh1 receiver_station=2"
            ),
        ),
        mk(
            "DENM",
            "DENM_DELIVER_OK",
            veh="veh1",
            node=1,
            station=2,
            msg=(
                "sender=veh0 sender_station=1 "
                "receiver=veh1 receiver_station=2"
            ),
        ),
    ]

    m = rx_pipeline.compute(entries)

    assert m.decode_ok == 1
    assert m.replay_ok == 1
    assert m.reception_ok == 1
    assert m.deliver_ok == 1


def test_p18_mixed_cam_denm_runtime_semantics():
    from v2x_analyzer.metrics import errors, exit_criteria
    from v2x_analyzer.metrics.connectivity import ConnectivityMetrics
    from v2x_analyzer.metrics.duplicates import DuplicateMetrics
    from v2x_analyzer.metrics.rx_pipeline import RxPipelineMetrics
    from v2x_analyzer.metrics.tx_pipeline import TxPipelineMetrics

    tx = TxPipelineMetrics(
        generate_ok=2,
        encode_begin=1,
        encode_ok=2,
        sign_begin=2,
        sign_ok=2,
        btp_tx_begin=2,
        btp_tx_ok=2,
        geonet_tx_begin=2,
        geonet_tx_ok=2,
        access_tx_begin=2,
        access_tx_ok=1,
        send_begin=1,
        send_ok=1,
        tx_complete=1,
    )

    rx = RxPipelineMetrics(
        raw_rx=4,
        access_rx_ok=4,
        geonet_rx_ok=4,
        btp_rx_ok=4,
        verify_ok=4,
        decode_ok=3,
        replay_ok=2,
        replay_drop=1,
        reception_ok=2,
        deliver_ok=2,
    )

    conn = ConnectivityMetrics(
        n_vehicles=3,
        max_pairs=6,
        tx_complete=1,
        deliver_ok=2,
    )

    result = exit_criteria.compute(
        conn,
        tx,
        rx,
        DuplicateMetrics(),
    )

    assert result.all_pass
    assert errors.compute([
        mk(
            "DENM",
            "DENM_REPLAY_DROP",
            veh="veh1",
            node=1,
            station=2,
        )
    ]).total == 0


def test_p18_denm_replay_drop_is_counted_as_pipeline_filter():
    from v2x_analyzer.metrics import rx_pipeline

    m = rx_pipeline.compute([
        mk(
            "DENM",
            "DENM_REPLAY_DROP",
            veh="veh1",
            node=1,
            station=2,
        )
    ])

    assert m.replay_drop == 1


def test_p18_connectivity_counts_denm_delivery():
    from v2x_analyzer.metrics import connectivity

    entry = mk(
        "DENM",
        "DENM_DELIVER_OK",
        veh="veh1",
        node=1,
        station=2,
        msg="origin_station=1 action_seq=1",
    )

    identity = current_identity()

    conn = connectivity.compute(
        [entry],
        identity,
        tx_complete=1,
    )

    assert conn.deliver_ok == 1
    assert conn.active_pairs == {("veh0", "veh1")}


def test_p18_vehicle_metrics_count_denm_delivery():
    from v2x_analyzer.metrics import vehicles

    entry = mk(
        "DENM",
        "DENM_DELIVER_OK",
        veh="veh1",
        node=1,
        station=2,
        msg="origin_station=1 action_seq=1",
    )

    metrics = vehicles.compute(
        [entry],
        current_identity(),
    )

    assert metrics.rx_by_vehicle["veh1"] == 1


def test_p18_denm_decode_timing_uses_current_event():
    from v2x_analyzer.metrics import timing

    entry = mk(
        "DENM",
        "DENM_DECODE_OK",
        veh="veh1",
        node=1,
        station=2,
        duration=55,
    )

    metrics = timing.compute([entry])

    assert metrics.operations["decode"].samples == [55]
