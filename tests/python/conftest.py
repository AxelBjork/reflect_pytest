"""Session-scoped fixtures for the SIL test suite.

The binary path is resolved from the build directory relative to the repo root.
Override by setting the SIL_APP environment variable, e.g.:

    SIL_APP=/custom/path/sil_app pytest tests/python/
"""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

import pytest

_REPO_ROOT = Path(__file__).parents[2]
_DEFAULT_BIN = _REPO_ROOT / "build" / "sil_app"


def _sil_binary() -> Path:
    env_override = os.environ.get("SIL_APP")
    path = Path(env_override) if env_override else _DEFAULT_BIN
    if not path.exists():
        pytest.fail(
            f"sil_app binary not found at {path}.\n"
            "Run: cmake -B build -G Ninja && cmake --build build"
        )
    return path


@pytest.fixture(scope="session")
def sil_process():
    """Launch sil_app for the test session; terminate it afterwards."""
    binary = _sil_binary()
    proc = subprocess.Popen(
        [str(binary)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    # Wait for the startup banner so tests don't race the process.
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            pytest.fail(f"sil_app exited unexpectedly (rc={proc.returncode})")
        line = proc.stdout.readline()
        if "[sil_app] started" in line:
            break
        time.sleep(0.05)
    else:
        proc.kill()
        pytest.fail("sil_app did not print startup banner within 5 s")

    yield proc

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
