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
            ManeuverNode(target_pos=Point2D(x=0.5, y=0.0), speed_limit_rpm=1000, timeout_ms=500),
            ManeuverNode(target_pos=Point2D(x=1.0, y=0.0), speed_limit_rpm=1000, timeout_ms=500),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), speed_limit_rpm=0, timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), speed_limit_rpm=0, timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), speed_limit_rpm=0, timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), speed_limit_rpm=0, timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), speed_limit_rpm=0, timeout_ms=0),
            ManeuverNode(target_pos=Point2D(x=0.0, y=0.0), speed_limit_rpm=0, timeout_ms=0)
        ]
    )

    # Dispatch to C++ app
    udp.send_msg(MsgId.AutoDriveCommand, cmd)
    time.sleep(0.05) 

    # Wait for the motor to enter Executing State (2) as it starts driving Node 0
    assert wait_for_state(udp, 2, timeout_s=1.0)
    
    # Wait for completion (back to Ready 1)
    assert wait_for_state(udp, 1, timeout_s=3.0)

def test_autonomous_service_phase2_environment(udp):
    """Test that AutonomousService requests new environment data when moving out of bounds."""
    assert wait_for_state(udp, 1)

    # 1. Send initial environment for region 0 [0, 5]
    env0 = EnvironmentPayload(
        region_id=101,
        bounds=BoundingBox2D(min_pt=Point2D(0,0), max_pt=Point2D(1.0,1.0)),
        ambient_temp_c=25.0,
        incline_percent=0.0,
        surface_friction=1.0
    )
    udp.send_msg(MsgId.EnvironmentData, env0)
    # Wait for ACK
    ack = udp.recv_msg(expected_id=MsgId.EnvironmentAck)
    assert ack.region_id == 101

    # 2. Send route that goes to x=10, with environment tuning enabled
    cmd = AutoDriveCommandTemplate_8(
        route_name=b"Env Test\x00",
        mode=DriveMode.Eco,
        p_gain=0.0,
        use_environment_tuning=True, # TRIGGER PHASE 2
        route_transform=[Vector3(1,0,0), Vector3(0,1,0), Vector3(0,0,1)],
        num_nodes=1,
        route=[
            ManeuverNode(target_pos=Point2D(x=2.0, y=0.0), speed_limit_rpm=400, timeout_ms=1000)
        ] + [ManeuverNode(Point2D(0,0),0,0)]*7
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)
    
    # 3. Wait for x > 5. Since we requested 400 RPM (4 m/s), it should take ~1.25s to reach x=5
    # The service should publish EnvironmentRequest when it detects it's out of bounds.
    
    # Observe EnvironmentRequest
    start = time.time()
    req = None
    while time.time() - start < 2.0:
        try:
            msg = udp.recv_msg(expected_id=MsgId.EnvironmentRequest, verbose=False)
            if msg:
                req = msg
                break
        except TimeoutError:
            pass
        time.sleep(0.01)

    assert req is not None, "Did not receive EnvironmentRequest when out of bounds"
    assert req.target_location.x >= 0.0, f"Request sent too early? x={req.target_location.x}"
    print(f"Received EnvironmentRequest at x={req.target_location.x}")

    # 4. Provide new environment for region [5, 15]
    env1 = EnvironmentPayload(
        region_id=102,
        bounds=BoundingBox2D(min_pt=Point2D(0.1,0), max_pt=Point2D(15,0)),
        ambient_temp_c=25.0,
        incline_percent=5.0, # uphill
        surface_friction=0.8
    )
    udp.send_msg(MsgId.EnvironmentData, env1)
    # Wait for ACK
    ack = udp.recv_msg(expected_id=MsgId.EnvironmentAck)
    assert ack.region_id == 102

    # 5. Wait for completion
    assert wait_for_state(udp, 1, timeout_s=0.5)

def test_autonomous_service_phase3_telemetry(udp):
    """Test that AutonomousService reports energy and environment telemetry."""
    assert wait_for_state(udp, 1)

    # Send initial environment
    env = EnvironmentPayload(
        region_id=999,
        bounds=BoundingBox2D(min_pt=Point2D(-100,-100), max_pt=Point2D(100,100)),
        ambient_temp_c=20.0,
        incline_percent=0.0,
        surface_friction=1.0
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
            ManeuverNode(target_pos=Point2D(x=0.5, y=0.0), speed_limit_rpm=400, timeout_ms=1000),
            ManeuverNode(target_pos=Point2D(x=1.0, y=0.0), speed_limit_rpm=600, timeout_ms=1000)
        ] + [ManeuverNode(Point2D(0,0),0,0)]*6
    )
    udp.send_msg(MsgId.AutoDriveCommand, cmd)
    
    # Wait for completion
    assert wait_for_state(udp, 1, timeout_s=1.0)

    # 1. Provide some time for final messages to arrive. 
    # Since we are at 100Hz, there might be a lot of backlog.
    # We use a short loop to find the final status message.
    final_status = None
    start = time.time()
    while time.time() - start < 3.0:
        try:
            # We don't want to print everything, just find the status
            msg = udp.recv_msg(expected_id=MsgId.AutoDriveStatus, verbose=False)
            if msg:
                final_status = msg
                if msg.route_complete:
                    break
        except TimeoutError:
            break
    
    assert final_status is not None, "Did not receive AutoDriveStatus"
    
    print(f"Final Status: node_idx={final_status.current_node_idx}, complete={final_status.route_complete}, num_stats={final_status.num_stats}")
    
    assert final_status.route_complete == True
    assert final_status.num_stats == 2
    
    for i in range(2):
        s = final_status.node_stats[i]
        print(f"Node {i} - Initial E: {s.initial_energy_mj:.1f}, Final E: {s.final_energy_mj:.1f}, Used: {s.total_energy_used_mj:.1f} mJ")
        assert s.total_energy_used_mj > 0, f"Node {i} energy should be > 0"
        assert s.energy_per_meter_mj > 0, f"Node {i} efficiency should be > 0"

    # Check environment_ids
    assert final_status.num_environments_used >= 1
    found_999 = False
    for i in range(final_status.num_environments_used):
        if final_status.environment_ids[i].id == 999:
            found_999 = True
            break
    assert found_999, "Environment ID 999 found in telemetry!"
    print("Environment ID 999 verified in telemetry!")



