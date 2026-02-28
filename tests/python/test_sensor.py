import pytest
import time
from udp_client import UdpClient
from reflect_pytest.generated import (
    MsgId,
    SensorRequestPayload,
    EnvironmentPayload,
    Point2D,
    StateRequestPayload,
    AutoDriveCommandTemplate_8,
    DriveMode,
    Vector3,
    ManeuverNode,
)


def test_sensor_service_terrain(udp, wait_for_state_change):
    """Test SensorService path-dependent deterministic terrain generation."""
    # Ensure Ready state
    assert wait_for_state_change(1)

    # Send request for terrain at 0.0, 0.0
    req = SensorRequestPayload(target_location=Point2D(x=0.0, y=0.0))
    udp.send_msg(MsgId.SensorRequest, req)

    # We should get SensorAck back
    ack = udp.recv_msg(expected_id=MsgId.SensorAck, timeout_us=100_000)
    assert ack is not None
    assert ack.success == True

    # And then EnvironmentData
    env1 = udp.recv_msg(expected_id=MsgId.EnvironmentData, timeout_us=500_000)
    assert env1 is not None

    # Ask for same terrain again, should be identical
    udp.send_msg(MsgId.SensorRequest, req)
    ack2 = udp.recv_msg(expected_id=MsgId.SensorAck, timeout_us=100_000)
    assert ack2.success == True
    env2 = udp.recv_msg(expected_id=MsgId.EnvironmentData, timeout_us=500_000)
    assert env2 is not None

    assert env1.ambient_temp_c == env2.ambient_temp_c
    assert env1.incline_percent == env2.incline_percent
    assert env1.surface_friction == env2.surface_friction

    # Ask for different terrain, should be different
    req3 = SensorRequestPayload(target_location=Point2D(x=10.0, y=10.0))
    udp.send_msg(MsgId.SensorRequest, req3)
    ack3 = udp.recv_msg(expected_id=MsgId.SensorAck, timeout_us=100_000)
    assert ack3.success == True
    env3 = udp.recv_msg(expected_id=MsgId.EnvironmentData, timeout_us=500_000)
    assert env3 is not None

    assert env1.ambient_temp_c != env3.ambient_temp_c


def test_sensor_service_state_lock(udp, wait_for_state_change):
    """Test SensorService rejects requests when not Ready (e.g. Executing driving route)."""
    assert wait_for_state_change(1)  # Ensure Ready state

    req = SensorRequestPayload(target_location=Point2D(x=0.0, y=0.0))

    # 1. Request while Ready -> Success
    udp.send_msg(MsgId.SensorRequest, req)
    ack = udp.recv_msg(expected_id=MsgId.SensorAck, timeout_us=100_000)
    assert ack is not None and ack.success is True
    # Consume EventData to clear queue
    udp.recv_msg(expected_id=MsgId.EnvironmentData, timeout_us=500_000)

    # 2. Start a multi-node route
    cmd = AutoDriveCommandTemplate_8(
        route_name=b"State Lock Test\x00",
        mode=DriveMode.Eco,
        p_gain=0.0,
        use_environment_tuning=False,
        route_transform=[Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)],
        num_nodes=4,
        route=[
            ManeuverNode(target_pos=Point2D(x=1.0, y=0.0), timeout_ms=500),
            ManeuverNode(target_pos=Point2D(x=1.0, y=1.0), timeout_ms=500),
            ManeuverNode(target_pos=Point2D(x=0.0, y=1.0), timeout_ms=500),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=500),
        ]
        + [ManeuverNode(target_pos=Point2D(x=0, y=0), timeout_ms=0)] * 4,
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)

    # Wait for state to change to Executing (2)
    assert wait_for_state_change(2)

    # 3. Request while Executing -> Fail
    udp.send_msg(MsgId.SensorRequest, req)
    ack = udp.recv_msg(expected_id=MsgId.SensorAck, timeout_us=100_000)
    assert ack is not None
    assert ack.success is False
    assert ack.reason == 1

    # 4. Wait for route to finish and return to Ready (1)
    assert wait_for_state_change(1, timeout_us=2_000_000)

    # 5. Request again while Ready -> Success
    udp.send_msg(MsgId.SensorRequest, req)
    ack = udp.recv_msg(expected_id=MsgId.SensorAck, timeout_us=100_000)
    assert ack is not None and ack.success is True
    udp.recv_msg(expected_id=MsgId.EnvironmentData, timeout_us=500_000)
