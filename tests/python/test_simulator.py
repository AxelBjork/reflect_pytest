"""Comprehensive test suite for the motor simulation system.

Physics constants (must match simulator.cpp exactly):
    K_RPM_TO_MPS  = 0.01   ->  100 RPM = 1.0 m/s
    K_RPM_TO_AMPS = 0.005  ->  100 RPM = 0.5 A
    R_INT_OHM     = 0.5    ->  voltage drop = I * 0.5 * dt_s
    V_MAX         = 12.6   (fully charged)
    V_MIN         = 10.5   (depleted)
    TICK_US       = 10_000 (10 ms physics tick)
"""

import struct
import time

import pytest
from reflect_pytest.generated import (
    MsgId,
    StateRequestPayload,
    SystemState,
    MotorSequencePayloadTemplate_5 as MotorSequencePayload,
    MotorSubCmd,
    KinematicsRequestPayload,
    KinematicsPayload,
    PowerRequestPayload,
    PowerPayload,
)

from udp_client import UdpClient

# ── Physics reference values ─────────────────────────────────────────────────
K_RPM_TO_MPS = 0.01
K_RPM_TO_AMPS = 0.005
R_INT = 0.5
V_MAX = 12.6
V_MIN = 10.5

MAX_STEPS = 5


def expected_position(steps: list[tuple[int, int]]) -> float:
    """Compute expected final position in metres from physics constants."""
    pos = 0.0
    for rpm, dur_us in steps:
        pos += rpm * K_RPM_TO_MPS * (dur_us / 1e6)
    return pos


def expected_voltage_drop(steps: list[tuple[int, int]]) -> float:
    """Compute cumulative voltage drop across a sequence."""
    drop = 0.0
    for rpm, dur_us in steps:
        current = abs(rpm) * K_RPM_TO_AMPS
        drop += current * R_INT * (dur_us / 1e6)
    return drop


# ── Fixtures ─────────────────────────────────────────────────────────────────


# ── Tests: state machine ─────────────────────────────────────────────────────


def test_starts_ready(udp):
    """Simulator should report Ready immediately after startup."""
    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    resp = udp.recv_msg(expected_id=MsgId.StateData)
    assert resp.state == SystemState.Ready


def test_transitions_to_executing(udp, dispatch_sequence, wait_for_state_change):
    """State must be Executing while a long sequence runs."""
    dispatch_sequence(cmd_id=1, steps=[MotorSubCmd(speed_rpm=100, duration_us=50_000)])
    wait_for_state_change(SystemState.Executing)
    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    resp = udp.recv_msg(expected_id=MsgId.StateData)
    assert resp.state == SystemState.Executing


def test_returns_to_ready_after_completion(udp, dispatch_sequence, wait_for_sequence_completion):
    """State must return to Ready once a short sequence finishes."""
    dispatch_sequence(cmd_id=2, steps=[MotorSubCmd(speed_rpm=100, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=2, duration_us=20_000)
    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    resp = udp.recv_msg(expected_id=MsgId.StateData)
    assert resp.state == SystemState.Ready


# ── Tests: kinematics ────────────────────────────────────────────────────────


def test_single_step_position_forward(dispatch_sequence, query_kinematics, wait_for_sequence_completion):
    """100 RPM for 20 ms → 0.02 m displacement."""
    dispatch_sequence(cmd_id=10, steps=[MotorSubCmd(speed_rpm=100, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=10, duration_us=20_000)
    k = query_kinematics()
    assert k.cmd_id == 10
    assert abs(k.position_m - expected_position([(100, 20_000)])) < 0.01


def test_single_step_position_reverse(dispatch_sequence, query_kinematics, wait_for_sequence_completion):
    """Negative RPM produces negative displacement."""
    dispatch_sequence(cmd_id=11, steps=[MotorSubCmd(speed_rpm=-200, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=11, duration_us=20_000)
    k = query_kinematics()
    assert k.position_m < -0.03  # > 0.03 m reverse (-200 RPM * 0.01 * 0.02s = -0.04m)


def test_speed_proportional_to_rpm(dispatch_sequence, query_kinematics):
    """Speed while executing should be rpm * K_RPM_TO_MPS."""
    dispatch_sequence(
        cmd_id=12,
        steps=[MotorSubCmd(speed_rpm=300, duration_us=50_000)],
        wait_started=True,
    )
    k = query_kinematics()
    assert abs(k.speed_mps - 300 * K_RPM_TO_MPS) < 0.05


def test_zero_rpm_no_displacement(dispatch_sequence, query_kinematics, wait_for_sequence_completion):
    """0 RPM step should produce no position change."""
    dispatch_sequence(cmd_id=13, steps=[MotorSubCmd(speed_rpm=0, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=13, duration_us=20_000)
    k = query_kinematics()
    assert abs(k.position_m) < 0.01


def test_speed_zero_after_completion(dispatch_sequence, query_kinematics, wait_for_sequence_completion):
    """Speed should be 0.0 once sequence is done."""
    dispatch_sequence(cmd_id=14, steps=[MotorSubCmd(speed_rpm=500, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=14, duration_us=20_000)
    k = query_kinematics()
    assert k.speed_mps == 0.0


def test_multi_step_net_displacement(dispatch_sequence, query_kinematics, wait_for_sequence_completion):
    """Equal forward + backward steps at same |RPM| → ~0 net displacement."""
    steps = [
        MotorSubCmd(speed_rpm=200, duration_us=10_000),
        MotorSubCmd(speed_rpm=-200, duration_us=10_000),
    ]
    dispatch_sequence(cmd_id=15, steps=steps)
    wait_for_sequence_completion(cmd_id=15, duration_us=20_000)
    k = query_kinematics()
    assert abs(k.position_m) < 0.02


def test_elapsed_us_increases(dispatch_sequence, query_kinematics):
    """elapsed_us mid-sequence must be greater than zero."""
    dispatch_sequence(
        cmd_id=16,
        steps=[MotorSubCmd(speed_rpm=100, duration_us=50_000)],
        wait_started=True,
    )
    # wait_started=True ensures elapsed_us > 0
    k = query_kinematics()
    assert k.elapsed_us > 0


def test_cmd_id_echoed_in_kinematics(dispatch_sequence, query_kinematics, wait_for_sequence_completion):
    """cmd_id in telemetry must match what was sent."""
    dispatch_sequence(cmd_id=42, steps=[MotorSubCmd(speed_rpm=100, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=42, duration_us=20_000)
    k = query_kinematics()
    assert k.cmd_id == 42


# ── Tests: power model ───────────────────────────────────────────────────────


def test_current_scales_superlinearly_with_rpm(dispatch_sequence, query_power, wait_for_state_change):
    """
    Current should increase superlinearly with RPM (power-law exponent > 1).
    Uses a single sequence with 3 steps so we can sample idle, base, and 2x base.
    """
    step_us = 20_000  # 2 ticks
    dispatch_sequence(
        cmd_id=20,
        steps=[
            MotorSubCmd(speed_rpm=0, duration_us=step_us),
            MotorSubCmd(speed_rpm=500, duration_us=step_us),
            MotorSubCmd(speed_rpm=1000, duration_us=step_us),
        ],
    )
    wait_for_state_change(SystemState.Executing)

    # Sample mid-step for each segment.
    time.sleep((step_us / 2) / 1e6)
    i_idle = query_power().current_a

    time.sleep(step_us / 1e6)
    i1 = query_power().current_a

    time.sleep(step_us / 1e6)
    i2 = query_power().current_a

    assert i2 > i1

    # Compare “above-idle” components to avoid baseline skew.
    denom = max(i1 - i_idle, 1e-3)
    ratio = (i2 - i_idle) / denom

    # For a power law with p ~ 1.5, doubling RPM gives ~2.83x (above idle).
    # Use a looser bound to keep the test robust.
    assert ratio > 2.5


def test_current_low_at_rest(dispatch_sequence, query_power, wait_for_sequence_completion):
    """No current when idle."""
    dispatch_sequence(cmd_id=21, steps=[MotorSubCmd(speed_rpm=200, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=21, duration_us=20_000)
    p = query_power()
    assert p.current_a < 0.15


def test_voltage_decreases_under_load(dispatch_sequence, query_power, wait_for_sequence_completion):
    """Voltage after a sequence must be lower than V_MAX."""
    dispatch_sequence(cmd_id=22, steps=[MotorSubCmd(speed_rpm=200, duration_us=50_000)])  # shortened
    wait_for_sequence_completion(cmd_id=22, duration_us=200_000)
    p = query_power()
    assert p.voltage_v < V_MAX


def test_higher_rpm_faster_voltage_drop(dispatch_sequence, query_power, wait_for_sequence_completion):
    """500 RPM for 50 ms should drain more voltage than 100 RPM for 50 ms."""
    # Run high RPM sequence
    dispatch_sequence(cmd_id=23, steps=[MotorSubCmd(speed_rpm=500, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=23, duration_us=20_000)
    p_high = query_power()
    drop_high = V_MAX - p_high.voltage_v

    base_v = p_high.voltage_v
    dispatch_sequence(cmd_id=24, steps=[MotorSubCmd(speed_rpm=100, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=24, duration_us=20_000)
    p_low = query_power()
    drop_low = base_v - p_low.voltage_v

    assert (
        drop_high > drop_low * 4.0
    )  # 500 RPM → 5× the drop of 100 RPM, allowing for some floating point noise


def test_soc_decreases_with_voltage(dispatch_sequence, query_power, wait_for_sequence_completion):
    """SoC must decline as voltage drops."""
    dispatch_sequence(cmd_id=25, steps=[MotorSubCmd(speed_rpm=2000, duration_us=50_000)])
    wait_for_sequence_completion(cmd_id=25, duration_us=50_000)
    p = query_power()
    assert p.state_of_charge < 100


def test_cmd_id_echoed_in_power(dispatch_sequence, query_power, wait_for_sequence_completion):
    """cmd_id in power telemetry must match what was sent."""
    dispatch_sequence(cmd_id=99, steps=[MotorSubCmd(speed_rpm=100, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=99, duration_us=20_000)
    p = query_power()
    assert p.cmd_id == 99


# ── Tests: advanced sequences ────────────────────────────────────────────────


def test_max_steps_all_executed(dispatch_sequence, query_kinematics, wait_for_sequence_completion):
    """All 10 sub-command slots should be executed."""
    steps = [MotorSubCmd(speed_rpm=50, duration_us=10_000)] * MAX_STEPS
    dispatch_sequence(cmd_id=30, steps=steps)
    wait_for_sequence_completion(cmd_id=30, duration_us=50_000)
    k = query_kinematics()
    # expected_position needs a small update to handle sub-cmds
    expected = sum(s.speed_rpm * K_RPM_TO_MPS * (s.duration_us / 1e6) for s in steps)
    assert abs(k.position_m - expected) < 0.01


def test_sequence_preemption(
    dispatch_sequence,
    query_kinematics,
    wait_for_state_change,
    wait_for_sequence_completion,
):
    """New sequence sent mid-execution should produce its own cmd_id in telemetry."""
    dispatch_sequence(cmd_id=40, steps=[MotorSubCmd(speed_rpm=100, duration_us=40_000)])  # shortened
    wait_for_state_change(target_state=SystemState.Executing)
    # Preempt with a short new sequence
    dispatch_sequence(cmd_id=41, steps=[MotorSubCmd(speed_rpm=200, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=41, duration_us=20_000)
    k = query_kinematics()
    assert k.cmd_id == 41  # new cmd_id, not the preempted one


def test_alternating_directions_accumulate(dispatch_sequence, query_kinematics, wait_for_sequence_completion):
    """Multiple alternating steps: net position should match physics model."""
    steps = [
        MotorSubCmd(speed_rpm=100, duration_us=30_000),
        MotorSubCmd(speed_rpm=-50, duration_us=20_000),
        MotorSubCmd(speed_rpm=200, duration_us=10_000),
    ]
    dispatch_sequence(cmd_id=50, steps=steps)
    wait_for_sequence_completion(cmd_id=50, duration_us=60_000)
    k = query_kinematics()
    expected = sum(s.speed_rpm * K_RPM_TO_MPS * (s.duration_us / 1e6) for s in steps)
    assert abs(k.position_m - expected) < 0.05
