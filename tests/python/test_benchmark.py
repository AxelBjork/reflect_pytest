"""Benchmark: measure per-test sil_app startup + QueryState roundtrip time."""

import time
import os
import pytest
from reflect_pytest.generated import (
    MsgId,
    StateRequestPayload,
    SystemState,
    InternalEnvRequestPayload,
    EnvironmentPayload,
    BoundingBox2D,
    Point2D,
    KinematicsRequestPayload,
    PowerRequestPayload,
    ThermalRequestPayload,
    AutoDriveCommandTemplate_8,
    MotorSequencePayloadTemplate_5,
    MotorSubCmd,
    Vector3,
    ManeuverNode,
)
from udp_client import UdpClient


def _wait_for_ready(proc) -> float:
    """Returns seconds until StateData Ready, or raises on timeout/crash."""
    sock_client = UdpClient()
    sock_client._sock.settimeout(0.001)
    sock_client.register()

    t0 = time.monotonic()
    deadline = t0 + 5.0
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            sock_client.close()
            raise RuntimeError(f"sil_app exited (rc={proc.returncode})")
        try:
            sock_client.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
            response = sock_client.recv_msg(expected_id=MsgId.StateData)
            if response and response.state == SystemState.Ready:
                elapsed = time.monotonic() - t0
                sock_client.close()
                return elapsed
        except TimeoutError:
            pass

    sock_client.close()
    raise TimeoutError("SIL did not report Ready within 5 s")


@pytest.mark.parametrize("n", range(1, 4))
def test_startup_time(n, sil_process):
    """Each iteration launches sil_app and times the StateRequest handshake."""
    t0 = time.monotonic()
    ready_after = _wait_for_ready(sil_process)
    total = time.monotonic() - t0
    print(f"  [{n:3d}] ready={ready_after*1000:.2f}ms  total={total*1000:.2f}ms")


@pytest.mark.parametrize(
    "req_id, req_payload, resp_id",
    [
        (MsgId.StateRequest, StateRequestPayload(reserved=0), MsgId.StateData),
        (
            MsgId.KinematicsRequest,
            KinematicsRequestPayload(reserved=0),
            MsgId.KinematicsData,
        ),
        (MsgId.PowerRequest, PowerRequestPayload(reserved=0), MsgId.PowerData),
        (MsgId.ThermalRequest, ThermalRequestPayload(reserved=0), MsgId.ThermalData),
        (
            MsgId.EnvironmentData,
            EnvironmentPayload(
                region_id=1,
                bounds=BoundingBox2D(min_pt=Point2D(0, 0), max_pt=Point2D(10, 10)),
                ambient_temp_c=25.0,
                incline_percent=0.0,
                surface_friction=1.0,
                max_speed_rpm=1000.0,
            ),
            MsgId.EnvironmentAck,
        ),
    ],
)
def test_ipc_rtt_benchmark(udp, req_id, req_payload, resp_id):
    """Measure RTT for various request-response pairs."""
    iterations = 200  # Stable count for RTT
    latencies = []

    print(f"\nRTT Benchmark: {req_id.name} -> {resp_id.name} ({iterations} iterations)")
    for _ in range(iterations):
        start = time.perf_counter()
        udp.send_msg(req_id, req_payload, verbose=False)
        udp.recv_msg(expected_id=resp_id, verbose=False)
        latencies.append(time.perf_counter() - start)

    avg_ms = (sum(latencies) / iterations) * 1000
    print(f"  Result: Avg={avg_ms:.3f}ms, Min={min(latencies)*1000:.3f}ms, Max={max(latencies)*1000:.3f}ms")


def test_ipc_profile_breakdown(udp):
    """Analyze CPU time breakdown (User vs System) for 100k messages."""
    count = 10000
    print(f"\nStarting Profile Trace ({count} messages)...")

    t_start = os.times()
    start_wall = time.perf_counter()

    for i in range(count):
        udp.send_msg(
            MsgId.InternalEnvRequest,
            InternalEnvRequestPayload(x=float(i), y=0.0),
            verbose=False,
        )

    end_wall = time.perf_counter()
    t_end = os.times()

    user_s = t_end.user - t_start.user
    sys_s = t_end.system - t_start.system
    real_s = end_wall - start_wall

    print(f"Profile breakdown for {count} messages:")
    print(f"  Wall Time:   {real_s:.3f}s")
    print(f"  User CPU:    {user_s:.3f}s (Python + library logic)")
    print(f"  System CPU:  {sys_s:.3f}s (Kernel syscall overhead)")
    print(f"  Total CPU:   {user_s + sys_s:.3f}s")
    if user_s > 0:
        print(f"  Kernel/User Ratio: {sys_s/user_s:.2f}")
    print(f"  Throughput:  {count/real_s:.1f} msgs/sec")


def test_command_override_latency(udp):
    """Measure latency from MotorSequence submission to KinematicsData update."""
    iterations = 100
    latencies = []

    print(f"\nCommand Override Latency Benchmark ({iterations} iterations)")

    # Payload for a simple maneuver
    steps = [MotorSubCmd(speed_rpm=1000, duration_us=1_000_000)] + [
        MotorSubCmd(speed_rpm=0, duration_us=0)
    ] * 4

    for i in range(1, iterations + 1):
        cmd_id = 1000 + i
        payload = MotorSequencePayloadTemplate_5(cmd_id=cmd_id, num_steps=1, steps=steps)

        start = time.perf_counter()
        udp.send_msg(MsgId.MotorSequence, payload, verbose=False)

        # Poll KinematicsData until cmd_id matches
        match = False
        while not match:
            udp.send_msg(
                MsgId.KinematicsRequest,
                KinematicsRequestPayload(reserved=0),
                verbose=False,
            )
            try:
                resp = udp.recv_msg(expected_id=MsgId.KinematicsData, verbose=False, timeout_us=100_000)
                if resp and resp.cmd_id == cmd_id:
                    match = True
            except TimeoutError:
                pass

        latencies.append(time.perf_counter() - start)

    avg_ms = (sum(latencies) / iterations) * 1000
    print(f"  Result: Avg={avg_ms:.3f}ms, Min={min(latencies)*1000:.3f}ms, Max={max(latencies)*1000:.3f}ms")
