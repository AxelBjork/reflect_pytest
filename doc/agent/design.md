# Software Design - Reflect Pytest

## Overview

`reflect_pytest` is a Software-in-the-Loop (SIL) test framework designed to bridge the gap between low-level C++ simulation logic and high-level Python testing without the burden of manual glue code. It leverages **C++26 Static Reflection** (P2996) to automatically generate introspection metadata for IPC messages.

## Core Design Principles

### 1. Reflection-Driven Integration
The "Single Source of Truth" is the C++ `messages.h` header. By reflecting over the `MsgId` enum and its associated payload types, the system automatically:
- Generates Python bindings via `C++26 reflection`.
- Orchestrates UDP serialization/deserialization.
- Produces comprehensive protocol documentation.

### 2. Service-Oriented Architecture (SOA)
The C++ application is composed of modular, decoupled services (e.g., `MotorService`, `KinematicsService`, `LogService`).
- **Isolation**: Each service has its own private state and logic.
- **Dependency Injection**: Services receive their dependencies (like `MessageBus` or `LogService`) via their constructors, making them easier to test and reason about.
- **Manifests**: Services declare their communication interface using `Subscribes` and `Publishes` tags, which are aggregate `MsgList` types.

### 3. Asynchronous IPC
Communication between services happens over a `MessageBus` (backed by `AF_UNIX` datagram sockets).
- **Non-blocking**: Publishing a message is a fast, non-blocking operation (copying into the socket buffer).
- **Decoupling**: Publishers don't know who is listening.
- **Bridging**: The `UdpBridge` acts as a generic relay, forwarding bus traffic to external UDP listeners (like `pytest`).

### 4. Specialized Logging Pipeline
Logging is handled by a dedicated `LogService` acting as an asynchronous sink.
- **`ComponentLogger` Proxy**: Services use a lightweight proxy that offloads the heavy work (string formatting, I/O) to the `LogService` worker thread.
- **Centralized Output**: `LogService` handles both console output and broadcasting `LogPayload` messages to the network.

## Message Patterns

### Request-Response
Used for state queries (e.g., `KinematicsRequest` -> `KinematicsData`). Requests are typically 1-byte sentinels to minimize bandwidth, while responses carry the full state snapshot.

### Unidirectional Streams
Used for telemetry and logging (e.g., `LogMessage`, `PhysicsTick`). These are broadcasted by services and consumed by any interested subscriber without explicit coordination.

### Timed Sequences
Used for command execution (e.g., `MotorSequence`). These payloads carry a schedule of sub-commands that the destination service executes in real-time.

## Dev Experience

- **Single Entry Point**: `pytest` orchestrates everything from C++ compilation to assertion checking.
- **Live Observability**: Using `pytest -s` allows streaming live C++ logs directly into the test output buffer.
- **Auto-Docs**: The system remains self-documenting; any change to a message struct is immediately reflected in the IPC reference.
