"""Benchmark: measure per-test sil_app startup + QueryState roundtrip time."""

import time

import pytest
from reflect_pytest.generated import MsgId, StateRequestPayload, SystemState
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
            sock_client.send_msg(MsgId.StateRequest,
                                 StateRequestPayload(reserved=0))
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
    print(
        f"  [{n:3d}] ready={ready_after*1000:.2f}ms  total={total*1000:.2f}ms")
