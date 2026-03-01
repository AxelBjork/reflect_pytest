import hashlib
import os
import pytest
from udp_client import UdpClient
from reflect_pytest.generated import MsgId, RevisionRequestPayload


def test_protocol_revision_hash(udp):
    """Test RevisionService returns the same hash that matches Python's generated bindings."""
    # 1. Ask for the revision hash
    req = RevisionRequestPayload(reserved=0)
    udp.send_msg(MsgId.RevisionRequest, req)

    # 2. Wait for response
    resp = udp.recv_msg(expected_id=MsgId.RevisionResponse, timeout_us=100_000)
    assert resp is not None

    returned_hash = resp.protocol_hash.decode("utf-8").rstrip("\x00")

    # 3. Read our locally generated hash
    from reflect_pytest.generated import PROTOCOL_HASH

    # 4. Compare
    assert (
        returned_hash == PROTOCOL_HASH
    ), f"Version mismatch! C++ thinks it's {returned_hash}, Python thinks it's {PROTOCOL_HASH}"
