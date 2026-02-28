from __future__ import annotations
import time
import pytest
from reflect_pytest.generated import (
    MsgId,
    SystemState,
    StateRequestPayload,
    KinematicsRequestPayload,
    PowerRequestPayload,
    MotorSequencePayloadTemplate_5 as MotorSequencePayload,
    MotorSubCmd,
    EnvironmentPayload,
    Point2D,
    BoundingBox2D,
    ThermalRequestPayload,
)

MAX_STEPS = 5


@pytest.fixture
def dispatch_sequence(udp):
    """Fixture that returns a helper to send a MotorSequence."""

    def _dispatch(
        cmd_id: int,
        steps: list[MotorSubCmd],
        wait_accepted: bool = False,
        wait_started: bool = False,
        wait_complete: bool = False,
    ):
        padding_needed = MAX_STEPS - len(steps)
        padded_steps = steps + [MotorSubCmd(0, 0)] * padding_needed
        payload = MotorSequencePayload(cmd_id=cmd_id, num_steps=len(steps), steps=padded_steps)
        udp.send_msg(MsgId.MotorSequence, payload)

        if wait_accepted or wait_started or wait_complete:
            _wait_for_acceptance(udp, cmd_id)

        if wait_started or wait_complete:
            _wait_for_started(udp, cmd_id)

        if wait_complete:
            total_us = sum(s.duration_us for s in steps)
            _wait_for_completion(udp, cmd_id, total_us)

    return _dispatch


@pytest.fixture
def query_kinematics(udp):
    """Fixture that returns a helper to query kinematics telemetry."""

    def _query(verbose: bool = True):
        udp.send_msg(
            MsgId.KinematicsRequest,
            KinematicsRequestPayload(reserved=0),
            verbose=verbose,
        )
        return udp.recv_msg(expected_id=MsgId.KinematicsData, verbose=verbose)

    return _query


@pytest.fixture
def query_power(udp):
    """Fixture that returns a helper to query power telemetry."""

    def _query(verbose: bool = True):
        udp.send_msg(MsgId.PowerRequest, PowerRequestPayload(reserved=0), verbose=verbose)
        return udp.recv_msg(expected_id=MsgId.PowerData, verbose=verbose)

    return _query


@pytest.fixture
def wait_for_acceptance(udp):
    return lambda cmd_id, timeout_us=1_000_000: _wait_for_acceptance(udp, cmd_id, timeout_us)


@pytest.fixture
def wait_for_started(udp):
    return lambda cmd_id, timeout_us=1_000_000: _wait_for_started(udp, cmd_id, timeout_us)


@pytest.fixture
def wait_for_sequence_completion(udp):
    return lambda cmd_id, duration_us: _wait_for_completion(udp, cmd_id, duration_us)


@pytest.fixture
def wait_for_state_change(udp):
    """Fixture that returns a helper to wait for a specific system state."""
    return lambda target_state, timeout_us=1_000_000: _wait_for_state(udp, target_state, timeout_us)


@pytest.fixture
def query_thermal(udp):
    """Fixture that returns a helper to query thermal telemetry."""

    def _query(verbose: bool = True):
        udp.send_msg(MsgId.ThermalRequest, ThermalRequestPayload(reserved=0), verbose=verbose)
        return udp.recv_msg(expected_id=MsgId.ThermalData, verbose=verbose)

    return _query


@pytest.fixture
def set_environment(udp):
    """Fixture that returns a helper to set environment conditions."""

    def _set(
        ambient_temp: float,
        incline: float,
        friction: float,
        max_speed_rpm: int,
        region_id: int = 0,
    ):
        # EnvironmentData is inbound (sent by Python, consumed by App)
        env = EnvironmentPayload(
            region_id=region_id,
            bounds=BoundingBox2D(min_pt=Point2D(x=0, y=0), max_pt=Point2D(x=0, y=0)),
            ambient_temp_c=ambient_temp,
            incline_percent=incline,
            surface_friction=friction,
            max_speed_rpm=max_speed_rpm,
        )
        udp.send_msg(MsgId.EnvironmentData, env)

        # EnvironmentService ACKs receipt to confirm it's been processed
        ack = udp.recv_msg(expected_id=MsgId.EnvironmentAck)
        assert ack.region_id == region_id
        return env

    return _set


def _wait_for_acceptance(udp, cmd_id, timeout_us=1_000_000):
    deadline = time.monotonic() + (timeout_us / 1e6)
    while time.monotonic() < deadline:
        udp.send_msg(MsgId.KinematicsRequest, KinematicsRequestPayload(reserved=0), verbose=False)
        try:
            k = udp.recv_msg(expected_id=MsgId.KinematicsData, verbose=False)
            if k and k.cmd_id == cmd_id:
                return k
        except TimeoutError:
            pass
    raise TimeoutError(f"Simulator did not accept sequence cmd_id={cmd_id}")


def _wait_for_started(udp, cmd_id, timeout_us=1_000_000):
    deadline = time.monotonic() + (timeout_us / 1e6)
    while time.monotonic() < deadline:
        udp.send_msg(MsgId.KinematicsRequest, KinematicsRequestPayload(reserved=0), verbose=False)
        try:
            k = udp.recv_msg(expected_id=MsgId.KinematicsData, verbose=False)
            if k and k.cmd_id == cmd_id and k.elapsed_us > 0:
                return k
        except TimeoutError:
            pass
    raise TimeoutError(f"Sequence {cmd_id} did not start moving")


def _wait_for_completion(udp, cmd_id, duration_us):
    deadline = time.monotonic() + (duration_us / 1e6) + 0.05
    while time.monotonic() < deadline:
        udp.drain()
        try:
            udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0), verbose=False)
            resp = udp.recv_msg(expected_id=MsgId.StateData, verbose=False)
            if resp and resp.state == SystemState.Ready:
                return
        except TimeoutError:
            pass
    raise TimeoutError(f"Simulator did not finalize sequence cmd_id={cmd_id}")


def _wait_for_state(udp, target_state, timeout_us=1_000_000):
    deadline = time.monotonic() + (timeout_us / 1e6)
    while time.monotonic() < deadline:
        udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0), verbose=False)
        try:
            msg = udp.recv_msg(expected_id=MsgId.StateData, verbose=False)
            if msg and msg.state == target_state:
                return True
        except TimeoutError:
            pass
    return False
