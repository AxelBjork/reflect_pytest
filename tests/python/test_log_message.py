"""Intercept and inject LogMessages through the UDP bridge."""

import time

import pytest
from reflect_pytest.generated import (
    LogPayload,
    MsgId,
    QueryStatePayload,
    SystemState,
    ComponentId,
    Severity,
)
from udp_client import UdpClient


@pytest.fixture(scope="module")
def udp(sil_process):
    """Open a UdpClient, register with the bridge, and wait for SIL Ready state."""
    with UdpClient() as client:
        client.register()

        # Ping the SIL until it reports Ready
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            try:
                client.send_msg(MsgId.QueryState,
                                QueryStatePayload(state=SystemState.Init))
                response = client.recv_msg(expected_id=MsgId.QueryState)
                if response and response.state == SystemState.Ready:
                    break
            except TimeoutError:
                pass
            time.sleep(0.05)
        else:
            pytest.fail("SIL did not report SystemState.Ready in time")

        yield client


def test_intercept_hello_world(udp):
    """The C++ app sends 'Hello World' every 500 ms — we must receive one."""
    log = udp.recv_msg(expected_id=MsgId.Log)  # blocks up to 5 s
    assert b"Hello World" in log.text
    assert log.severity == Severity.Info
    assert log.component == ComponentId.Main


def test_inject_hello_world(udp):
    """Send our own Hello World; bridge injects it and echoes it back.
     UDP may drop packets, so resend periodically and check with a short timeout.
    """
    sent = LogPayload(
        text=b"Hello World from pytest\x00",
        severity=Severity.Info,
        component=ComponentId.Test,
    )

    # Temporarily shorten socket block so we can poll quickly and resend.
    udp._sock.settimeout(0.1)
    deadline = time.monotonic() + 5.0

    try:
        while time.monotonic() < deadline:
            udp.send_msg(MsgId.Log, sent)
            try:
                # Drain messages quickly
                log = udp.recv_msg(expected_id=MsgId.Log)
                if b"Hello World from pytest" in log.text and log.component == ComponentId.Test:
                    return  # found it
            except TimeoutError:
                pass
    finally:
        udp._sock.settimeout(5.0)

    pytest.fail(
        "Did not receive injected 'Hello World from pytest' back over UDP")
