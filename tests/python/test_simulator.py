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

# ── Helpers ──────────────────────────────────────────────────────────────────


def send_sequence(udp: UdpClient, cmd_id: int,
                  steps: list[tuple[int, int]]) -> None:
    sub_cmds = [MotorSubCmd(speed_rpm=r, duration_us=d) for r, d in steps]
    for _ in range(MAX_STEPS - len(steps)):
        sub_cmds.append(MotorSubCmd(speed_rpm=0, duration_us=0))
    payload = MotorSequencePayload(
        cmd_id=cmd_id,
        num_steps=len(steps),
        steps=sub_cmds,
    )
    udp.send_msg(MsgId.MotorSequence, payload)


def query_kinematics(udp: UdpClient) -> KinematicsPayload:
    drain_socket(udp)
    udp.send_msg(MsgId.KinematicsRequest, KinematicsRequestPayload(reserved=0))
    return udp.recv_msg(expected_id=MsgId.KinematicsData)


def query_power(udp: UdpClient) -> PowerPayload:
    drain_socket(udp)
    udp.send_msg(MsgId.PowerRequest, PowerRequestPayload(reserved=0))
    return udp.recv_msg(expected_id=MsgId.PowerData)


def drain_socket(udp: UdpClient) -> None:
    """Discard all buffered UDP packets so subsequent recv is fresh."""
    old_timeout = udp._sock.gettimeout()
    udp._sock.settimeout(0.0)
    try:
        while True:
            udp._sock.recvfrom(4096)
    except (BlockingIOError, TimeoutError):
        pass
    finally:
        udp._sock.settimeout(old_timeout)


def wait_for_sequence(udp: UdpClient,
                      cmd_id: int,
                      duration_us: int,
                      slack: float = 0.1) -> None:
    """Sleep for the sequence duration then do a single clean query."""
    time.sleep(duration_us / 1e6 + slack)
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        drain_socket(udp)
        try:
            udp.send_msg(MsgId.StateRequest,
                         StateRequestPayload(reserved=0))
            state_resp = udp.recv_msg(expected_id=MsgId.StateData)
            if state_resp and state_resp.state == SystemState.Ready:
                drain_socket(udp)
                udp.send_msg(MsgId.KinematicsRequest,
                             KinematicsRequestPayload(reserved=0))
                k = udp.recv_msg(expected_id=MsgId.KinematicsData)
                if k and k.cmd_id == cmd_id:
                    return
            time.sleep(0.01)
        except TimeoutError:
            pass
    raise TimeoutError(f"Simulator did not finish sequence cmd_id={cmd_id}")


def wait_executing(udp: UdpClient, timeout: float = 1.0) -> None:
    """Block until simulator enters Executing state."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        time.sleep(0.01)
        try:
            drain_socket(udp)
            udp.send_msg(MsgId.StateRequest,
                         StateRequestPayload(reserved=0))
            resp = udp.recv_msg(expected_id=MsgId.StateData)
            if resp and resp.state == SystemState.Executing:
                return
        except TimeoutError:
            pass
    raise TimeoutError("Simulator did not enter Executing in time")


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


@pytest.fixture
def udp(sil_process):
    """Fresh UDP client per test; waits for Ready before yielding."""
    with UdpClient(client_port=9002) as client:
        client.register()
        client._sock.settimeout(0.001)
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            if sil_process.poll() is not None:
                pytest.fail(f"sil_app exited (rc={sil_process.returncode})")
            try:
                client.send_msg(MsgId.StateRequest,
                                StateRequestPayload(reserved=0))
                resp = client.recv_msg(expected_id=MsgId.StateData)
                if resp and resp.state == SystemState.Ready:
                    break
            except TimeoutError:
                pass
        else:
            pytest.fail("SIL did not report Ready in time")
        client._sock.settimeout(1.0)
        yield client


# ── Tests: state machine ─────────────────────────────────────────────────────


def test_starts_ready(udp):
    """Simulator should report Ready immediately after startup."""
    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    resp = udp.recv_msg(expected_id=MsgId.StateData)
    assert resp.state == SystemState.Ready


def test_transitions_to_executing(udp):
    """State must be Executing while a long sequence runs."""
    send_sequence(udp, cmd_id=1, steps=[(100, 200_000)])
    wait_executing(udp)
    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    resp = udp.recv_msg(expected_id=MsgId.StateData)
    assert resp.state == SystemState.Executing


def test_returns_to_ready_after_completion(udp):
    """State must return to Ready once a short sequence finishes."""
    send_sequence(udp, cmd_id=2, steps=[(100, 50_000)])  # 50 ms
    wait_for_sequence(udp, cmd_id=2, duration_us=50_000, slack=0.1)
    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    resp = udp.recv_msg(expected_id=MsgId.StateData)
    assert resp.state == SystemState.Ready


# ── Tests: kinematics ────────────────────────────────────────────────────────


def test_single_step_position_forward(udp):
    """100 RPM for 50 ms → 0.05 m displacement."""
    send_sequence(udp, cmd_id=10, steps=[(100, 50_000)])
    wait_for_sequence(udp, cmd_id=10, duration_us=50_000)
    k = query_kinematics(udp)
    assert k.cmd_id == 10
    assert abs(k.position_m - 0.05) < 0.01  # ±20% of 0.05m


def test_single_step_position_reverse(udp):
    """Negative RPM produces negative displacement."""
    send_sequence(udp, cmd_id=11, steps=[(-200, 50_000)])
    wait_for_sequence(udp, cmd_id=11, duration_us=50_000)
    k = query_kinematics(udp)
    assert k.position_m < -0.09  # > 0.09 m reverse


def test_speed_proportional_to_rpm(udp):
    """Speed while executing should be rpm * K_RPM_TO_MPS."""
    send_sequence(udp, cmd_id=12, steps=[(300, 200_000)])
    wait_executing(udp)
    k = query_kinematics(udp)
    assert abs(k.speed_mps - 300 * K_RPM_TO_MPS) < 0.05


def test_zero_rpm_no_displacement(udp):
    """0 RPM step should produce no position change."""
    send_sequence(udp, cmd_id=13, steps=[(0, 50_000)])
    wait_for_sequence(udp, cmd_id=13, duration_us=50_000)
    k = query_kinematics(udp)
    assert abs(k.position_m) < 0.01


def test_speed_zero_after_completion(udp):
    """Speed should be 0.0 once sequence is done."""
    send_sequence(udp, cmd_id=14, steps=[(500, 50_000)])
    wait_for_sequence(udp, cmd_id=14, duration_us=50_000)
    k = query_kinematics(udp)
    assert k.speed_mps == 0.0


def test_multi_step_net_displacement(udp):
    """Equal forward + backward steps at same |RPM| → ~0 net displacement."""
    send_sequence(udp, cmd_id=15, steps=[(200, 50_000), (-200, 50_000)])
    wait_for_sequence(udp, cmd_id=15, duration_us=100_000)
    k = query_kinematics(udp)
    assert abs(k.position_m) < 0.05


def test_elapsed_us_increases(udp):
    """elapsed_us mid-sequence must be greater than zero."""
    send_sequence(udp, cmd_id=16, steps=[(100, 200_000)])
    wait_executing(udp)
    k = query_kinematics(udp)
    assert k.elapsed_us > 0


def test_cmd_id_echoed_in_kinematics(udp):
    """cmd_id in telemetry must match what was sent."""
    send_sequence(udp, cmd_id=42, steps=[(100, 20_000)])
    wait_for_sequence(udp, cmd_id=42, duration_us=20_000)
    k = query_kinematics(udp)
    assert k.cmd_id == 42


# ── Tests: power model ───────────────────────────────────────────────────────


def test_current_proportional_to_rpm(udp):
    """Current must equal |rpm| * K_RPM_TO_AMPS while executing."""
    send_sequence(udp, cmd_id=20, steps=[(400, 200_000)])
    wait_executing(udp)
    p = query_power(udp)
    assert abs(p.current_a - 400 * K_RPM_TO_AMPS) < 0.05


def test_current_zero_at_rest(udp):
    """No current when idle."""
    send_sequence(udp, cmd_id=21, steps=[(200, 20_000)])
    wait_for_sequence(udp, cmd_id=21, duration_us=20_000)
    p = query_power(udp)
    assert p.current_a == 0.0


def test_voltage_decreases_under_load(udp):
    """Voltage after a sequence must be lower than V_MAX."""
    send_sequence(udp, cmd_id=22, steps=[(200, 200_000)])  # 0.2 s at 200 RPM
    wait_for_sequence(udp, cmd_id=22, duration_us=200_000)
    p = query_power(udp)
    expected_drop = expected_voltage_drop([(200, 200_000)])
    assert p.voltage_v < V_MAX
    assert abs(p.voltage_v - (V_MAX - expected_drop)) < 0.05


def test_higher_rpm_faster_voltage_drop(udp):
    """500 RPM for 50 ms should drain more voltage than 100 RPM for 50 ms."""
    # Run high RPM sequence
    send_sequence(udp, cmd_id=23, steps=[(500, 50_000)])
    wait_for_sequence(udp, cmd_id=23, duration_us=50_000)
    p_high = query_power(udp)
    drop_high = V_MAX - p_high.voltage_v

    base_v = p_high.voltage_v
    send_sequence(udp, cmd_id=24, steps=[(100, 50_000)])
    wait_for_sequence(udp, cmd_id=24, duration_us=50_000)
    p_low = query_power(udp)
    drop_low = base_v - p_low.voltage_v

    assert drop_high > drop_low * 4.5  # 500 RPM → 5× the drop of 100 RPM


def test_soc_decreases_with_voltage(udp):
    """SoC must decline as voltage drops."""
    send_sequence(udp, cmd_id=25, steps=[(500, 200_000)])
    wait_for_sequence(udp, cmd_id=25, duration_us=200_000)
    p = query_power(udp)
    assert p.state_of_charge < 100


def test_cmd_id_echoed_in_power(udp):
    """cmd_id in power telemetry must match what was sent."""
    send_sequence(udp, cmd_id=99, steps=[(100, 20_000)])
    wait_for_sequence(udp, cmd_id=99, duration_us=20_000)
    p = query_power(udp)
    assert p.cmd_id == 99


# ── Tests: advanced sequences ────────────────────────────────────────────────


def test_max_steps_all_executed(udp):
    """All 10 sub-command slots should be executed."""
    # Each step: 50 RPM for 10 ms → 0.0005 m per step, 10 steps → 0.005 m
    steps = [(50, 10_000)] * MAX_STEPS
    send_sequence(udp, cmd_id=30, steps=steps)
    wait_for_sequence(udp, cmd_id=30, duration_us=100_000)
    k = query_kinematics(udp)
    expected = expected_position(steps)
    assert abs(k.position_m - expected) < 0.01


def test_sequence_preemption(udp):
    """New sequence sent mid-execution should produce its own cmd_id in telemetry."""
    send_sequence(udp, cmd_id=40, steps=[(100, 500_000)])  # 0.5 s
    wait_executing(udp)
    # Preempt with a short new sequence
    send_sequence(udp, cmd_id=41, steps=[(200, 50_000)])
    wait_for_sequence(udp, cmd_id=41, duration_us=50_000)
    k = query_kinematics(udp)
    assert k.cmd_id == 41  # new cmd_id, not the preempted one


def test_alternating_directions_accumulate(udp):
    """Multiple alternating steps: net position should match physics model."""
    steps = [(100, 30_000), (-50, 20_000), (200, 10_000)]
    send_sequence(udp, cmd_id=50, steps=steps)
    wait_for_sequence(udp, cmd_id=50, duration_us=60_000)
    k = query_kinematics(udp)
    expected = expected_position(steps)
    assert abs(k.position_m - expected) < 0.05
