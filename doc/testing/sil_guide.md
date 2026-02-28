# SIL Testing Guide

The Python harness acts as the primary "Network Peer" in the simulation. It connects to the C++ SIL application via UDP and exchanges binary messages as defined in the [IPC Protocol Reference](../ipc/protocol.md).

## Overview

The test suite is built on top of `pytest` and uses a custom `UdpClient` to communicate with the C++ `UdpBridge`.

### Key Components

- **`udp_client.py`**: A low-level UDP wrapper that handles message serialization/deserialization using the auto-generated types.
    - Handles [uint16_t msgId LE][payload bytes] wire format.
    - Includes an internal queue to handle out-of-order or asynchronous message arrival.
- **`fixtures_ipc.py`**: A collection of pytest fixtures for common IPC operations.
    - `dispatch_sequence`: Sends timed motor commands and waits for simulator state changes.
    - `wait_for_state_change`: Polls the C++ bus until a specific `SystemState` is reached.
- **`conftest.py`**: Orchestrates the C++ build process and manages the `sil_app` process lifetime.
    - Resolves binary path (defaulting to `build/sil_app`).
    - Handles `--build` and `--simulator` CLI flags.

## The Test Loop

1. **Setup**: `conftest.py` ensures the C++ app is built and running.
2. **Execution**: The test sends a command (e.g., `AutoDriveCommand`) via `udp.send_msg`.
3. **Verification**: The test waits for specific outcomes (e.g., `wait_for_state_change(Ready)`) or queries telemetry (`query_kinematics`).

## Breadcrumbs
[Home](../../README.md) | [Architecture](../arch/design.md) | [IPC Protocol](../ipc/protocol.md)
