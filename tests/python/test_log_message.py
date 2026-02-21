"""Intercept and inject LogMessages through the UDP bridge."""

import time

import pytest
from reflect_pytest.udp_client import (
    ComponentId,
    LogPayload,
    Severity,
    UdpClient,
)


@pytest.fixture(scope="module")
def udp(sil_process):
    """Open a UdpClient, register with the bridge, yield for the module."""
    with UdpClient() as client:
        client.register()
        # Give the bridge a moment to record our address before messages.
        time.sleep(0.1)
        yield client


def test_intercept_hello_world(udp):
    """The C++ app sends 'Hello World' every 500 ms — we must receive one."""
    log = udp.recv_log()  # blocks up to 5 s (UdpClient default timeout)
    assert "Hello World" in log.text
    assert log.severity == Severity.Info
    assert log.component == ComponentId.Main


def test_inject_hello_world(udp):
    """Send our own Hello World; bridge injects it and echoes it back."""
    sent = LogPayload(
        text="Hello World from pytest",
        severity=Severity.Info,
        component=ComponentId.Test,
    )
    udp.send_log(sent)

    # Drain messages until we see our own (the bridge may echo C++ messages).
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        try:
            log = udp.recv_log()
        except TimeoutError:
            break
        if log.text == sent.text and log.component == ComponentId.Test:
            return  # found it

    pytest.fail(
        "Did not receive injected 'Hello World from pytest' back over UDP")
