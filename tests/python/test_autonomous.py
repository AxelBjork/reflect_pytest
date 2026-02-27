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
    MotorSequencePayloadTemplate_5
)


def wait_for_state(udp: UdpClient, target_state: int, timeout_s: float = 1.0):
    start = time.time()
    while time.time() - start < timeout_s:
        udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
        try:
            msg = udp.recv_msg(expected_id=MsgId.StateData, verbose=False)
            if msg and msg.state == target_state:
                return True
        except TimeoutError:
            pass
        time.sleep(0.01)
    return False

def test_autonomous_service_phase1_route(udp):
    """Test that AutoDriveCommand is accurately serialized and the C++ AutonomousService dispatches MotorSequences."""
    assert wait_for_state(udp, 1)  # Ready

    # Construct complex nested struct payload
    cmd = AutoDriveCommandTemplate_8(
        route_name=b"Test Route 1\x00",
        mode=DriveMode.Eco,
        p_gain=0.0,
        use_environment_tuning=False,
        route_transform=[
            Vector3(1.0, 0.0, 0.0),
            Vector3(0.0, 1.0, 0.0),
            Vector3(0.0, 0.0, 1.0)
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
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), timeout_ms=0)
        ]
    )

    # Dispatch to C++ app
    udp.send_msg(MsgId.AutoDriveCommand, cmd)

    # Wait for the motor to enter Executing State (2) as it starts driving Node 0
    assert wait_for_state(udp, 2, timeout_s=1.0)
    
    # Wait for completion (back to Ready 1)
    assert wait_for_state(udp, 1, timeout_s=3.0)

def test_autonomous_service_phase2_environment(udp):
    """Test that AutonomousService requests new environment data when moving out of bounds."""
    assert wait_for_state(udp, 1)

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
            ManeuverNode(
                target_pos=Point2D(x=0.2, y=0.0),
                timeout_ms=100,
            )
        ]
        + [ManeuverNode(Point2D(0, 0), 0)] * 7,
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)

    # 3. Observe EnvironmentRequest (scaled timeout / sleep)
    start = time.time()
    req = None
    while time.time() - start < 0.3:
        try:
            msg = udp.recv_msg(expected_id=MsgId.EnvironmentRequest, verbose=False)
            if msg:
                req = msg
                break
        except TimeoutError:
            pass
        time.sleep(0.001)

    assert req is not None, "Did not receive EnvironmentRequest when out of bounds"
    assert req.target_location.x >= (env0.bounds.max_pt.x - 1e-3), (
        f"Request sent too early? x={req.target_location.x}"
    )
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
    assert wait_for_state(udp, 1, timeout_s=0.3)

def test_autonomous_service_phase3_telemetry(udp):
    """Test that AutonomousService reports energy and environment telemetry."""
    assert wait_for_state(udp, 1)

    # Send initial environment
    env = EnvironmentPayload(
        region_id=999,
        bounds=BoundingBox2D(min_pt=Point2D(-100,-100), max_pt=Point2D(100,100)),
        ambient_temp_c=20.0,
        incline_percent=0.0,
        surface_friction=1.0,
        max_speed_rpm=600.0
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
        use_environment_tuning=True, # Ensure we process environment
        route_transform=[Vector3(1,0,0), Vector3(0,1,0), Vector3(0,0,1)],
        num_nodes=2,
        route=[
            ManeuverNode(target_pos=Point2D(x=0.5, y=0.0), timeout_ms=1000),
            ManeuverNode(target_pos=Point2D(x=1.0, y=0.0), timeout_ms=1000)
        ] + [ManeuverNode(Point2D(0,0),0)]*6
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)
    
    # Wait for completion
    assert wait_for_state(udp, 1, timeout_s=2.0)

    # Find final status
    final_status = None
    start = time.time()
    while time.time() - start < 3.0:
        try:
            msg = udp.recv_msg(expected_id=MsgId.AutoDriveStatus, verbose=False)
            if msg:
                final_status = msg
                if msg.route_complete:
                    break
        except TimeoutError:
            break
    
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

def run_efficiency_mission(udp, mode):
    """Helper to run a mission and return energy density."""
    # Ensure Ready state
    assert wait_for_state(udp, 1)
    udp.drain()
    
    # Setup environment with 2000 RPM limit
    env = EnvironmentPayload(
        region_id=888,
        bounds=BoundingBox2D(min_pt=Point2D(-100,-100), max_pt=Point2D(100,100)),
        ambient_temp_c=20.0,
        incline_percent=0.0,
        surface_friction=1.0,
        max_speed_rpm=2000.0
    )
    udp.send_msg(MsgId.EnvironmentData, env)
    udp.recv_msg(expected_id=MsgId.EnvironmentAck)

    # Drive 100.0m for stable integration
    cmd = AutoDriveCommandTemplate_8(
        route_name=b"Efficiency Test\x00",
        mode=mode,
        p_gain=1.0,
        use_environment_tuning=True,
        route_transform=[Vector3(1,0,0), Vector3(0,1,0), Vector3(0,0,1)],
        num_nodes=1,
        route=[ManeuverNode(target_pos=Point2D(x=100.0, y=0.0), timeout_ms=20000)] + [ManeuverNode(Point2D(0,0),0)]*7
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)
    # Wait for completion
    assert wait_for_state(udp, 1, timeout_s=15.0)
    
    status = None
    start_wait = time.monotonic()
    while time.monotonic() - start_wait < 3.0:
        try:
            msg = udp.recv_msg(expected_id=MsgId.AutoDriveStatus)
            if msg and msg.route_complete:
                status = msg
                break
        except TimeoutError:
            pass
    
    assert status is not None, f"No status received for mode {mode}"
    return status.node_stats[0].energy_per_meter_mj

#def test_autonomous_eco_efficiency(udp):
#    eff = run_efficiency_mission(udp, DriveMode.Eco)
#    print(f"\nEco (1500 RPM): {eff:.1f} mJ/m")
#    pytest.eco_eff = eff
#
#def test_autonomous_performance_efficiency(udp):
#    eff = run_efficiency_mission(udp, DriveMode.Performance)
#    print(f"\nPerformance (2200 RPM): {eff:.1f} mJ/m")
#    
#    if hasattr(pytest, "eco_eff"):
#        eco = pytest.eco_eff
#        print(f"Comparison: Perf={eff:.1f}, Eco={eco:.1f}, Ratio={eff/eco:.2f}")
#        assert eff > eco * 1.15, f"Performance ({eff:.1f}) should be significantly less efficient than Eco ({eco:.1f})"
#    else:
#        print("Eco test result not found, skipping comparison.")
#    assert eff > 0



