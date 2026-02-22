"""Lifecycle smoke tests for the SIL application."""

import time


def test_process_is_alive(sil_process):
    """The binary should still be running after startup."""
    assert sil_process.poll() is None, "sil_app exited unexpectedly"


def test_process_stays_alive(sil_process):
    """After 100 ms the binary should still be running (no crash on start)."""
    time.sleep(0.1)
    assert sil_process.poll() is None, "sil_app crashed within 100 ms"
