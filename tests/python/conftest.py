"""Session-scoped fixtures for the SIL test suite.

The binary path is resolved from the build directory relative to the repo root.
Override by setting the SIL_APP environment variable, e.g.:

    SIL_APP=/custom/path/sil_app pytest tests/python/
"""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path
import importlib

import pytest
from reflect_pytest.generated import (
    MsgId,
    SystemState,
    StateRequestPayload,
    ResetRequestPayload,
)
from udp_client import UdpClient

_REPO_ROOT = Path(__file__).parents[2]
_BUILD_DIR = _REPO_ROOT / "build"
_DEFAULT_BIN = _BUILD_DIR / "sil_app"

def pytest_addoption(parser):
    parser.addoption(
        "--build",
        action="store_true",
        default=False,
        help="Run CMake configure, build, and CTest before the pytest suite.",
    )
    parser.addoption(
        "--build-only",
        action="store_true",
        default=False,
        help="Run C++ build and CTest, then exit without running Python "
        "tests.",
    )
    parser.addoption(
        "--simulator",
        action="store_true",
        default=False,
        help="Run sil_app in simulator mode (no pytest SIL suite), then exit.",
    )
    parser.addoption(
        "--sim-duration",
        action="store",
        type=int,
        default=5,
        help="Duration in seconds for simulator mode (default 5s).",
    )


def pytest_sessionstart(session):
    """Orchestrate C++ build and CTest, or run Simulator mode."""
    config = session.config

    traffic_log = _BUILD_DIR / "test_traffic.jsonl"
    if traffic_log.exists():
        traffic_log.unlink()

    if config.getoption("--build") or config.getoption("--build-only"):
        print("\n[pytest] Orchestrating C++ build and CTest...")
        try:
            # 1. CMake Configure
            subprocess.run(
                ["cmake", "-B", str(_BUILD_DIR), "-G", "Ninja"],
                cwd=_REPO_ROOT,
                check=True,
            )
            # 2. CMake Build
            subprocess.run(
                ["cmake", "--build",
                 str(_BUILD_DIR), "--", "-j8"],
                cwd=_REPO_ROOT,
                check=True,
            )
            # 3. CTest Unit Tests
            subprocess.run(
                [
                    "ctest", "--test-dir",
                    str(_BUILD_DIR), "--output-on-failure"
                ],
                cwd=_REPO_ROOT,
                check=True,
            )
            print("[pytest] C++ Build & CTest successful.\n")
            if config.getoption("--build-only"):
                pytest.exit("C++ build/CTest successful. Exiting.",
                            returncode=0)
        except subprocess.CalledProcessError as e:
            pytest.exit(f"C++ build or CTest failed (rc={e.returncode})",
                        returncode=1)

    if config.getoption("--simulator"):
        duration = config.getoption("--sim-duration")
        binary = _sil_binary()
        print(f"\n[pytest] Starting Simulator Mode for {duration} seconds...")
        print(f"[pytest] Running: {binary}\n")

        try:
            # Run without capturing so output goes straight to terminal
            proc = subprocess.Popen([str(binary)])

            # Briefly wait for SIL to bind UDP
            time.sleep(0.5)

            try:
                from reflect_pytest.generated import (
                    MsgId,
                    MotorSequencePayloadTemplate_5,
                    MotorSubCmd,
                )
                from udp_client import UdpClient

                with UdpClient() as udp:
                    udp.register()
                    import struct
                    # Demo seq: +1500 RPM (1s), Stop (1s), -1500 RPM (1s)
                    steps = [(500, 2_000_000), (1000, 2_000_000),
                             (1500, 2_000_000), (-500, 2_000_000),
                             (-1000, 2_000_000), (-1500, 2_000_000)]
                    sub_cmds = [MotorSubCmd(speed_rpm=r, duration_us=d) for r, d in steps]
                    for _ in range(5 - len(steps)):
                        sub_cmds.append(MotorSubCmd(speed_rpm=0, duration_us=0))
                    payload = MotorSequencePayloadTemplate_5(
                        cmd_id=1,
                        num_steps=len(steps),
                        steps=sub_cmds,
                    )
                    udp.send_msg(MsgId.MotorSequence, payload)
                    print("\n[pytest] Injected demo sequence "
                          "(cmd=1, 6 steps)\n")
            except Exception as e:
                print(f"[pytest] Could not send demo sequence: {e}")

            startTime = time.monotonic()

            while time.monotonic() - startTime < duration:
                if proc.poll() is not None:
                    print(f"\n[pytest] sil_app exited early "
                          f"(rc={proc.returncode})")
                    break
                time.sleep(0.1)

            if proc.poll() is None:
                print(f"\n[pytest] Simulator duration ({duration}s) reached. "
                      "Shutting down...")
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()

        except KeyboardInterrupt:
            print("\n[pytest] Simulator interrupted by user. Shutting down...")
            if proc.poll() is None:
                proc.terminate()
                proc.wait()

        # Exit pytest entirely, skipping test collection and execution
        pytest.exit("Simulator mode finished.", returncode=0)


def _sil_binary() -> Path:
    env_override = os.environ.get("SIL_APP")
    path = Path(env_override) if env_override else _DEFAULT_BIN
    if not path.exists():
        pytest.fail(f"sil_app binary not found at {path}.\n"
                    "Run: cmake -B build -G Ninja && cmake --build build")
    return path


@pytest.fixture(scope="session")
def sil_process():
    """Launch sil_app for the test session; terminate it afterwards."""
    proc = subprocess.Popen([str(_sil_binary())])
    yield proc
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


@pytest.fixture(scope="session")
def udp(sil_process):
    """Fresh UDP client per test; waits for Ready before yielding."""
    with UdpClient(client_port=0) as client:
        client.register()
        client._sock.settimeout(0.001)
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            if sil_process.poll() is not None:
                pytest.fail(f"sil_app exited (rc={sil_process.returncode})")
            try:
                client.send_msg(MsgId.StateRequest,
                                StateRequestPayload(reserved=0))
                resp = client.recv_msg(expected_id=MsgId.StateData)
                if resp and resp.state == SystemState.Ready:
                    break
            except TimeoutError:
                pass
            time.sleep(0.001)
        else:
            pytest.fail("SIL did not report Ready in time")
        client._sock.settimeout(1.0)
        yield client


@pytest.fixture(scope="function", autouse=True)
def reset_sim(udp):
    """Automatically reset simulation state before every test."""
    udp.send_msg(MsgId.ResetRequest, ResetRequestPayload(reserved=0))
    # Tiny sleep to ensure C++ processes it before the next command
    time.sleep(0.001)
