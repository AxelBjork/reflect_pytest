import pytest
import struct
import time
from reflect_pytest.generated import (
    MsgId,
    StateRequestPayload,
    StatePayload,
    SystemState,
    LogPayload,
    Severity,
)
from udp_client import UdpClient


def test_bridge_enforcement(udp):
    """Verifies all enforcement logic in a single fast pass."""

    # 1. Authorized + Correct Size -> Success
    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    resp = udp.recv_msg(expected_id=MsgId.StateData)
    assert resp is not None

    # 2. Unauthorized Message -> Dropped
    # Use StateData (Bridge subscribes to it but does NOT publish it)
    udp.drain()

    udp.send_msg(MsgId.StateData, StatePayload(state=SystemState.Ready))
    with pytest.raises(TimeoutError):
        udp.recv_msg(expected_id=MsgId.StateData, timeout_us=5_000)

    # 3. Authorized Message + Wrong Size -> Dropped
    # StateRequest is correct at 1 byte. We send 2 bytes.
    msg_id_bytes = struct.pack("<H", int(MsgId.StateRequest))
    payload_bytes = b"\x00\xff"
    udp._sock.sendto(msg_id_bytes + payload_bytes, udp._bridge)

    with pytest.raises(TimeoutError):
        # If it were allowed, StateService would respond with StateData
        udp.recv_msg(expected_id=MsgId.StateData, timeout_us=5_000)

    # 4. Blocked External Message (EnvRequest)
    # The bridge should no longer publish EnvRequest from external sources
    from reflect_pytest.generated import EnvironmentRequestPayload, Point2D

    udp.send_msg(
        MsgId.EnvironmentRequest,
        EnvironmentRequestPayload(target_location=Point2D(0, 0)),
    )
    with pytest.raises(TimeoutError):
        # If it were allowed, EnvironmentService used to respond with EnvironmentData
        udp.recv_msg(expected_id=MsgId.EnvironmentData, timeout_us=5_000)

    # 5. Final Sanity: Still operational
    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    assert udp.recv_msg(expected_id=MsgId.StateData, timeout_us=10_000) is not None
