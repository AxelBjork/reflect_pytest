import pytest
import time
from udp_client import UdpClient
from reflect_pytest.generated import (
    MsgId,
    StateRequestPayload,
    AutoDriveCommandTemplate_8,
    DriveMode,
    Point2D,
    Vector3,
    BoundingBox2D,
    ManeuverNode,
    EnvironmentPayload,
    EnvironmentRequestPayload,
    AutoDriveStatusTemplate_8__4,
)


def test_autonomous_service_phase1_route(udp, wait_for_state_change):
    """Test that AutoDriveCommand is accurately serialized and the C++ AutonomousService dispatches MotorSequences."""
    assert wait_for_state_change(1)  # Ready

    # Construct complex nested struct payload
    cmd = AutoDriveCommandTemplate_8(
        route_name=b"Test Route 1\x00",
        mode=DriveMode.Eco,
        p_gain=0.0,
        use_environment_tuning=False,
        route_transform=[
            Vector3(1.0, 0.0, 0.0),
            Vector3(0.0, 1.0, 0.0),
            Vector3(0.0, 0.0, 1.0),
        ],
        num_nodes=2,
        route=[
            ManeuverNode(target_pos=Point2D(x=0.05, y=0.0), timeout_ms=50),
            ManeuverNode(target_pos=Point2D(x=0.1, y=0.0), timeout_ms=50),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=0),
        ],
    )

    # Dispatch to C++ app
    udp.send_msg(MsgId.AutoDriveCommand, cmd)

    # Wait for the motor to enter Executing State (2) as it starts driving Node 0
    assert wait_for_state_change(2, timeout_us=1_000_000)

    # Wait for completion (back to Ready 1)
    assert wait_for_state_change(1, timeout_us=3_000_000)


def test_autonomous_service_phase2_environment(udp, wait_for_state_change):
    """Test that AutonomousService requests new environment data when moving out of bounds."""
    assert wait_for_state_change(1)

    # 1. Send initial environment for region 0 (scaled)
    env0 = EnvironmentPayload(
        region_id=101,
        bounds=BoundingBox2D(
            min_pt=Point2D(0, 0),
            max_pt=Point2D(0.1, 1.0),
        ),
        ambient_temp_c=25.0,
        incline_percent=0.0,
        surface_friction=1.0,
        max_speed_rpm=400.0,
    )
    udp.send_msg(MsgId.EnvironmentData, env0)
    ack = udp.recv_msg(expected_id=MsgId.EnvironmentAck)
    assert ack.region_id == 101

    # 2. Send route that goes out of bounds (scaled), with environment tuning enabled
    cmd = AutoDriveCommandTemplate_8(
        route_name=b"Env Test\x00",
        mode=DriveMode.Eco,
        p_gain=0.0,
        use_environment_tuning=True,  # TRIGGER PHASE 2
        route_transform=[Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)],
        num_nodes=1,
        route=[
            ManeuverNode(target_pos=Point2D(x=0.2, y=0.0), timeout_ms=100),
        ]
        + [ManeuverNode(target_pos=Point2D(x=0, y=0), timeout_ms=0)] * 7,
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)

    # 3. Observe EnvironmentRequest (passive wait)
    try:
        req = udp.recv_msg(expected_id=MsgId.EnvironmentRequest, verbose=False, timeout_us=300_000)
    except TimeoutError:
        req = None

    assert req is not None, "Did not receive EnvironmentRequest when out of bounds"
    assert (
        req.target_location.x >= env0.bounds.max_pt.x - 1e-3
    ), f"Request sent too early? x={req.target_location.x}"
    print(f"Received EnvironmentRequest at x={req.target_location.x}")

    # 4. Provide new environment for next region (scaled)
    env1 = EnvironmentPayload(
        region_id=102,
        bounds=BoundingBox2D(
            min_pt=Point2D(0.01, 0),
            max_pt=Point2D(1.5, 0),
        ),
        ambient_temp_c=25.0,
        incline_percent=5.0,  # uphill
        surface_friction=0.8,
        max_speed_rpm=200.0,
    )
    udp.send_msg(MsgId.EnvironmentData, env1)
    ack = udp.recv_msg(expected_id=MsgId.EnvironmentAck)
    assert ack.region_id == 102

    # 5. Wait for completion (scaled)
    assert wait_for_state_change(1, timeout_us=300_000)


def test_autonomous_service_phase3_telemetry(udp, wait_for_state_change):
    """Test that AutonomousService reports energy and environment telemetry."""
    assert wait_for_state_change(1)

    # Send initial environment
    env = EnvironmentPayload(
        region_id=999,
        bounds=BoundingBox2D(min_pt=Point2D(-100, -100), max_pt=Point2D(100, 100)),
        ambient_temp_c=20.0,
        incline_percent=0.0,
        surface_friction=1.0,
        max_speed_rpm=6000.0,
    )
    udp.send_msg(MsgId.EnvironmentData, env)
    # Wait for ACK
    ack = udp.recv_msg(expected_id=MsgId.EnvironmentAck)
    assert ack.region_id == 999

    # Send route with 2 nodes
    cmd = AutoDriveCommandTemplate_8(
        route_name=b"Telemetry Test\x00",
        mode=DriveMode.Performance,
        p_gain=0.0,
        use_environment_tuning=True,  # Ensure we process environment
        route_transform=[Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)],
        num_nodes=2,
        route=[
            ManeuverNode(target_pos=Point2D(x=0.5, y=0.0), timeout_ms=500),
            ManeuverNode(target_pos=Point2D(x=1.0, y=0.0), timeout_ms=500),
        ]
        + [ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=0)] * 6,
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)

    # Wait for completion
    assert wait_for_state_change(1, timeout_us=100_000)
    # Find final status
    try:
        # Loop until route_complete to skip intermediate heartbeat statuses
        while True:
            msg = udp.recv_msg(expected_id=MsgId.AutoDriveStatus, verbose=False, timeout_us=3_000_000)
            if msg and msg.route_complete:
                final_status = msg
                break
    except TimeoutError:
        final_status = None

    assert final_status is not None, "Did not receive AutoDriveStatus"
    print(f"Final Status: node_idx={final_status.current_node_idx}, complete={final_status.route_complete}")

    assert final_status.route_complete == True
    assert final_status.num_stats == 2

    for i in range(2):
        s = final_status.node_stats[i]
        print(f"Node {i} efficiency: {s.energy_per_meter_mj:.1f} mJ/m")
        assert s.energy_per_meter_mj > 0

    # Check environment_ids
    assert final_status.num_environments_used >= 1
    found_999 = False
    for i in range(final_status.num_environments_used):
        if final_status.environment_ids[i].id == 999:
            found_999 = True
            break
    assert found_999, "Environment ID 999 found in telemetry!"


def run_efficiency_mission(udp, mode, wait_for_state_change, start_x=0.0):
    """Helper to run a mission and return energy density."""
    # Ensure Ready state
    assert wait_for_state_change(1)
    udp.drain()

    # Setup environment with 5000 RPM limit for speedup
    env = EnvironmentPayload(
        region_id=888,
        bounds=BoundingBox2D(min_pt=Point2D(-100, -100), max_pt=Point2D(100, 100)),
        ambient_temp_c=20.0,
        incline_percent=0.0,
        surface_friction=1.0,
        max_speed_rpm=5000.0,
    )
    udp.send_msg(MsgId.EnvironmentData, env)
    udp.recv_msg(expected_id=MsgId.EnvironmentAck)

    target_x = start_x + 5.0  # Drive 5.0m
    cmd = AutoDriveCommandTemplate_8(
        route_name=b"Efficiency Test\x00",
        mode=mode,
        p_gain=1.0,
        use_environment_tuning=True,
        route_transform=[
            Vector3(x=1, y=0, z=0),
            Vector3(x=0, y=1, z=0),
            Vector3(x=0, y=0, z=1),
        ],
        num_nodes=1,
        route=[ManeuverNode(target_pos=Point2D(x=target_x, y=0.0), timeout_ms=500)]
        + [ManeuverNode(target_pos=Point2D(x=0, y=0), timeout_ms=0)] * 7,
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)

    # 1. Wait for Execution to start
    assert wait_for_state_change(2, timeout_us=50_000)
    # 2. Wait for completion
    assert wait_for_state_change(1, timeout_us=300_000)

    try:
        while True:
            # The status must be in the queue if we missed it during state wait
            msg = udp.recv_msg(expected_id=MsgId.AutoDriveStatus, timeout_us=1_000)
            if msg and msg.route_complete:
                status = msg
                break
    except TimeoutError:
        status = None

    assert status is not None, f"No status received for mode {mode}"
    return status.node_stats[0].energy_per_meter_mj


def test_autonomous_efficiency_comparison(udp, wait_for_state_change):
    """Compare Eco vs Performance modes using 5.0m ultra-high-speed maneuvers."""
    # Run 1: 0.0 -> 5.0 (Eco)
    eco = run_efficiency_mission(udp, DriveMode.Eco, wait_for_state_change, start_x=0.0)
    print(f"\nEco: {eco:.1f} mJ/m")

    # Run 2: 5.0 -> 10.0 (Performance)
    perf = run_efficiency_mission(udp, DriveMode.Performance, wait_for_state_change, start_x=5.0)
    print(f"Performance: {perf:.1f} mJ/m")

    # Comparison
    print(f"Efficiency Ratio (Perf/Eco): {perf/eco:.2f}")

    # Still expect at least 10% difference on short run
    assert perf > eco * 1.10, f"Performance ({perf:.1f}) should be more expensive than Eco ({eco:.1f})"
    print("Efficiency scaling verified!")
