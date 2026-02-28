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


def test_environment_override(set_environment):
    """Test that setting environment conditions is acknowledged by the application."""
    resp = set_environment(ambient_temp=30.5, incline=5.0, friction=0.8, max_speed_rpm=100)
    assert resp.ambient_temp_c == 30.5
    assert resp.incline_percent == 5.0
    assert resp.surface_friction == pytest.approx(0.8)
    assert resp.max_speed_rpm == 100


def test_thermal_increases_under_load(
    set_environment, dispatch_sequence, query_thermal, wait_for_sequence_completion
):
    """Motor and battery temps should increase when driving."""
    set_environment(ambient_temp=20.0, incline=0.0, friction=1.0, max_speed_rpm=100)
    initial_thermal = query_thermal()

    # Run a high RPM sequence
    dispatch_sequence(cmd_id=101, steps=[MotorSubCmd(speed_rpm=800, duration_us=50_000)])  # shortened
    wait_for_sequence_completion(cmd_id=101, duration_us=50_000)

    final_thermal = query_thermal()

    assert final_thermal.motor_temp_c > initial_thermal.motor_temp_c
    assert final_thermal.battery_temp_c > initial_thermal.battery_temp_c


def test_thermal_cooldown(set_environment, dispatch_sequence, query_thermal, wait_for_sequence_completion):
    """Temperatures should drop back towards ambient over time when idle."""
    set_environment(ambient_temp=10.0, incline=0.0, friction=1.0, max_speed_rpm=100)
    # Heat it up significantly
    dispatch_sequence(cmd_id=102, steps=[MotorSubCmd(speed_rpm=800, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=102, duration_us=20_000)

    hot_thermal = query_thermal()
    assert hot_thermal.motor_temp_c > 10.0

    # Let it cool down with a zero-rpm sequence to trigger PhysicsTicks
    dispatch_sequence(cmd_id=103, steps=[MotorSubCmd(speed_rpm=0, duration_us=20_000)])
    wait_for_sequence_completion(cmd_id=103, duration_us=20_000)

    cool_thermal = query_thermal()
    assert cool_thermal.motor_temp_c < hot_thermal.motor_temp_c
    assert cool_thermal.battery_temp_c < hot_thermal.battery_temp_c
