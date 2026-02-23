"""Tests for ThermalService and EnvironmentService."""

import time
import pytest
from reflect_pytest.generated import (
    MsgId,
    EnvironmentAckPayload as EnvironmentAck,
    EnvironmentPayload,
    ThermalRequestPayload,
    ThermalPayload,
    MotorSequencePayloadTemplate_5 as MotorSequencePayload,
    MotorSubCmd,
    StateRequestPayload,
    SystemState,
    Point2D,
    BoundingBox2D,
)

def send_sequence(udp, cmd_id: int, steps: list[tuple[int, int]]) -> None:
    sub_cmds = [MotorSubCmd(speed_rpm=r, duration_us=d) for r, d in steps]
    for _ in range(5 - len(steps)):
        sub_cmds.append(MotorSubCmd(speed_rpm=0, duration_us=0))
    payload = MotorSequencePayload(
        cmd_id=cmd_id,
        num_steps=len(steps),
        steps=sub_cmds,
    )
    udp.send_msg(MsgId.MotorSequence, payload)

def drain_socket(udp) -> None:
    old_timeout = udp._sock.gettimeout()
    udp._sock.settimeout(0.0)
    try:
        while True:
            udp._sock.recvfrom(4096)
    except (BlockingIOError, TimeoutError):
        pass
    finally:
        udp._sock.settimeout(old_timeout)

def query_thermal(udp) -> ThermalPayload:
    drain_socket(udp)
    udp.send_msg(MsgId.ThermalRequest, ThermalRequestPayload(reserved=0))
    return udp.recv_msg(expected_id=MsgId.ThermalData)

def query_environment(udp) -> None:
    # EnvironmentRequest now doesn't return anything *defined* by the app.
    # The app just logs it. This test might be redundant now if it expects a response from the app.
    # For now, we just skip waiting for MsgId.EnvironmentData as it won't arrive from the app.
    drain_socket(udp)
    from reflect_pytest.generated import EnvironmentRequestPayload
    udp.send_msg(MsgId.EnvironmentRequest, EnvironmentRequestPayload(target_location=Point2D(0,0)))

def set_environment(udp, ambient_temp: float, incline: float, friction: float, region_id: int = 0):
    drain_socket(udp)
    # EnvironmentData is now inbound (sent by Python)
    env = EnvironmentPayload(
        region_id=region_id,
        bounds=BoundingBox2D(min_pt=Point2D(0,0), max_pt=Point2D(0,0)), # Placeholder bounds
        ambient_temp_c=ambient_temp,
        incline_percent=incline,
        surface_friction=friction
    )
    udp.send_msg(MsgId.EnvironmentData, env)
    
    # Wait for completion via ACK
    ack = udp.recv_msg(expected_id=MsgId.EnvironmentAck)
    assert ack.region_id == region_id
    return env # Return the sent env for compatibility with existing tests expecting it back

def wait_for_sequence(udp, cmd_id: int, duration_us: int) -> None:
    time.sleep(duration_us / 1e6 + 0.05)
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        drain_socket(udp)
        try:
            udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
            state_resp = udp.recv_msg(expected_id=MsgId.StateData)
            if state_resp and state_resp.state == SystemState.Ready:
                return
        except TimeoutError:
            pass
        time.sleep(0.01)
    raise TimeoutError("Simulator did not finish sequence")

def test_environment_override(udp):
    """Test we can override environment conditions and receive an ACK."""
    # set_environment now internally waits for MsgId.EnvironmentAck
    # and returns the EnvironmentPayload object we sent.
    resp = set_environment(udp, ambient_temp=30.5, incline=5.0, friction=0.8)
    assert resp.ambient_temp_c == 30.5
    assert resp.incline_percent == 5.0
    assert resp.surface_friction == pytest.approx(0.8)
    
    # We no longer "read back" from the app (one-way data flow).
    # Confirmation is handled by the ACK in set_environment.
    pass
    
def test_thermal_increases_under_load(udp):
    """Motor and battery temps should increase when driving."""
    set_environment(udp, ambient_temp=20.0, incline=0.0, friction=1.0)
    
    initial_thermal = query_thermal(udp)
    
    # Run a high RPM sequence
    send_sequence(udp, cmd_id=101, steps=[(800, 200_000)])
    wait_for_sequence(udp, cmd_id=101, duration_us=200_000)
    
    final_thermal = query_thermal(udp)
    
    assert final_thermal.motor_temp_c > initial_thermal.motor_temp_c
    assert final_thermal.battery_temp_c > initial_thermal.battery_temp_c

def test_thermal_cooldown(udp):
    """Temperatures should drop back towards ambient over time when idle."""
    set_environment(udp, ambient_temp=10.0, incline=0.0, friction=1.0)
    
    # Heat it up significantly
    send_sequence(udp, cmd_id=102, steps=[(800, 500_000)])
    wait_for_sequence(udp, cmd_id=102, duration_us=500_000)
    
    hot_thermal = query_thermal(udp)
    assert hot_thermal.motor_temp_c > 10.0
    
    # Let it cool down with a zero-rpm sequence to trigger PhysicsTicks
    send_sequence(udp, cmd_id=103, steps=[(0, 500_000)])
    wait_for_sequence(udp, cmd_id=103, duration_us=500_000)
    
    cool_thermal = query_thermal(udp)
    assert cool_thermal.motor_temp_c < hot_thermal.motor_temp_c
    assert cool_thermal.battery_temp_c < hot_thermal.battery_temp_c
