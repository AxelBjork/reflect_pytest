"""Intercept and inject LogMessages through the UDP bridge."""

import time

import pytest
from reflect_pytest.generated import (
    LogPayload,
    MsgId,
    StateRequestPayload,
    SystemState,
    Severity,
)
from udp_client import UdpClient


@pytest.fixture(scope="function")
def udp(sil_process):
    """Open a UdpClient, register with the bridge, and wait for SIL Ready state."""
    with UdpClient() as client:
        client.register()

        # 1ms socket timeout: loopback StateRequest roundtrip is ~100µs.
        client._sock.settimeout(0.001)
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            if sil_process.poll() is not None:
                pytest.fail(f"sil_app exited (rc={sil_process.returncode})")
            try:
                client.send_msg(MsgId.StateRequest,
                                StateRequestPayload(reserved=0))
                response = client.recv_msg(expected_id=MsgId.StateData)
                if response and response.state == SystemState.Ready:
                    break
            except TimeoutError:
                pass
        else:
            pytest.fail("SIL did not report SystemState.Ready in time")

        client._sock.settimeout(1.0)
        yield client


def test_inject_hello_world(udp):
    """Send our own Hello World; bridge injects it and echoes it back.

    UDP may drop packets, so resend on every poll iteration with a short timeout.
    """
    sent = LogPayload(
        text=b"Hello World from pytest\x00",
        severity=Severity.Info,
        component=b"test",
    )

    # 1ms poll: loopback echo is near-instant, retry on drop.
    udp._sock.settimeout(0.001)
    deadline = time.monotonic() + 5.0
    try:
        while time.monotonic() < deadline:
            udp.send_msg(MsgId.Log, sent)
            try:
                log = udp.recv_msg(expected_id=MsgId.Log)
                if b"Hello World from pytest" in log.text and b"test" in log.component:
                    return
            except TimeoutError:
                pass
    finally:
        udp._sock.settimeout(1.0)

    pytest.fail(
        "Did not receive injected 'Hello World from pytest' back over UDP")
