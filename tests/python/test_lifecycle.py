"""Lifecycle smoke tests for the SIL application."""

import time
from reflect_pytest.generated import MsgId, StateRequestPayload


def test_process_is_alive(sil_process):
    """The binary should still be running after startup."""
    assert sil_process.poll() is None, "sil_app exited unexpectedly"


def test_process_stays_alive(sil_process, udp):
    """After IPC registration the binary should still be running."""
    # Registering with UDP bridge confirms basic startup without hard sleep
    assert sil_process.poll() is None, "sil_app crashed during startup/IPC registration"
    # Basic ping to confirm app is responsive

    udp.send_msg(MsgId.StateRequest, StateRequestPayload(reserved=0))
    assert udp.recv_msg(timeout_us=50_000) is not None
