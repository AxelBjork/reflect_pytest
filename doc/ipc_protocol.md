# IPC Protocol Reference

> **Auto-generated** by `generate_docs` using C++26 static reflection (P2996 + P3394).
> Do not edit by hand — re-run `cmake --build build --target generate_docs` to refresh.

This document describes the full wire protocol used between the Python test harness
and the C++ SIL (Software-in-the-Loop) application.

## System Architecture

```
┌─────────────────────┐          UDP (port 9000)         ┌─────────────────────┐
│   Python / pytest   │ ──────────────────────────────── │     sil_app (C++)   │
│                     │   [uint16_t msgId][payload bytes] │                     │
│  udp_client.py      │                                   │  UdpBridge          │
│  generated.py       │                                   │  ├─ MessageBus      │
└─────────────────────┘                                   │  ├─ Simulator       │
                                                          │  └─ Logger          │
                                                          └─────────────────────┘
```

**Wire format:** Every datagram starts with a `uint16_t` message ID (host-byte
order) followed immediately by the fixed-size payload struct (packed, no padding).
If `sizeof(received payload) != sizeof(Payload)` the message is silently discarded.

**Threads (C++ side):**

| Thread | Purpose |
|---|---|
| `main` | Waits on shutdown signal; futex sleep |
| `heartbeat` | Publishes `LogPayload` "Hello World #N" every 500 ms |
| `bus-listener` | AF\_UNIX recv loop → dispatch to subscribers |
| `sim-exec` | Steps through `MotorSequencePayload` in real time |
| `sim-log` | Publishes kinematics status log every 1 000 ms |
| `bridge-rx` | UDP recv → injects into MessageBus |

---

## Message Flow

This diagram gives three distinct columns: `Pytest` uses the `UdpClient` module to orchestrate test cases, `Network` maps the transport layer across two explicit sockets (`Client -> App` and `App -> Client`), and `Simulator` processes the messages internally.

```mermaid
flowchart LR
    %% ─── COLUMN 1: Pytest Test Harness ───
    subgraph py["Pytest SIL Environment"]
        TestCase(["Test Case / Fixtures"])
    end

    %% ─── COLUMN 2: Python UDP Client & Kernel Routing ───
    subgraph net["UdpClient (127.0.0.1)"]
        direction TB
        TX(["TX Socket<br/>Port 9000<br/>==============<br/>(sendto)"])
        RX(["RX Socket<br/>Port 9001<br/>==============<br/>(recvfrom)"])
    end

    %% ─── COLUMN 3: C++ SUT ───
    subgraph sim["Simulator"]
        MotorService(["MotorService<br/><br/>Manages the thread that executes<br/>timed motor commands in real-time."])
        KinematicsService(["KinematicsService<br/><br/>Simulates vehicle motion by<br/>integrating motor RPM over time to<br/>track position and linear velocity."])
        PowerService(["PowerService<br/><br/>Models a simple battery pack<br/>dynamically responding to motor<br/>load."])
        StateService(["StateService<br/><br/>Maintains the central lifecycle<br/>state machine of the simulation."])
        LogService(["LogService<br/><br/>Periodically aggregates system<br/>state into human-readable text logs<br/>for debugging."])
        UdpBridge(["UdpBridge<br/><br/>Stateful bridge that relays IPC<br/>messages between the internal<br/>MessageBus and external UDP<br/>clients."])
        MainPublisher(["MainPublisher"])

        %% Pure Internal Messages
        MotorService -.->|PhysicsTick,<br/>StateChange| KinematicsService
        MotorService -.->|PhysicsTick,<br/>StateChange| PowerService
        MotorService -.->|StateChange| StateService
        MotorService -.->|PhysicsTick,<br/>StateChange| LogService

        %% Bridge Distribution
        LogService -->|Log| UdpBridge
        MainPublisher -->|Log| UdpBridge
        UdpBridge -->|QueryState| StateService
        StateService -->|QueryState| UdpBridge
        UdpBridge -->|MotorSequence| MotorService
        UdpBridge -->|KinematicsRequest| KinematicsService
        KinematicsService -->|KinematicsData| UdpBridge
        UdpBridge -->|PowerRequest| PowerService
        PowerService -->|PowerData| UdpBridge
    end

    %% ─── INBOUND PATH ───
    TestCase -->|send_msg| TX
    TX -->|"QueryState<br/>MotorSequence<br/>KinematicsRequest<br/>PowerRequest"| UdpBridge

    %% ─── OUTBOUND PATH ───
    UdpBridge -->|"Log<br/>QueryState<br/>KinematicsData<br/>PowerData"| RX
    RX -->|recv_msg| TestCase
```

---

## Component Services

The application is composed of the following services:

### `MotorService`

> Manages the thread that executes timed motor commands in real-time.

This service is responsible for stepping through a sequence of motor commands, emitting standard `PhysicsTick` events at 100Hz, and broadcasting `StateChange` events when a sequence starts or finishes.

### `KinematicsService`

> Simulates vehicle motion by integrating motor RPM over time to track position and linear velocity.

The physics model applies a linear conversion from RPM to meters-per-second, and integrates this velocity over the `PhysicsTick` delta-time to continuously evaluate the vehicle's position:

$$ v = \text{RPM} \times 0.01 \text{ (m/s)} $$

$$ x = \int v \, dt $$

### `PowerService`

> Models a simple battery pack dynamically responding to motor load.

The simulation calculates the current drawn by the motor based on its speed, then applies Ohm's law over the internal resistance to calculate the instantaneous voltage drop. The state of charge (SOC) is linearly interpolated between the maximum and minimum voltage limits:

$$ I = |\text{RPM}| \times 0.005 \text{ (A)} $$

$$ V \mathrel{-}= I \times R_{int} \times dt $$

$$ SOC = \frac{V - V_{min}}{V_{max} - V_{min}} \times 100 $$

### `StateService`

> Maintains the central lifecycle state machine of the simulation.

This component passively tracks the top-level simulated system state (Init, Ready, Executing, Fault) by listening to internal state transitions and makes it available to external clients via ping requests.

### `LogService`

> Periodically aggregates system state into human-readable text logs for debugging.

It acts as an internal observer, keeping track of the latest kinematics, power, and state metrics, and broadcasts a formatted string representation at a fixed interval to track the simulation's progress.

### `UdpBridge`

> Stateful bridge that relays IPC messages between the internal MessageBus and external UDP clients.

It remembers the IP address and port of the last connected test harness and bidirectionally routes all subscribed C++ events out through the UDP socket while safely injecting incoming UDP datagrams onto the internal MessageBus.

---

## Message Payloads

- [`Log`](#msgidlog-logpayload)
- [`QueryState`](#msgidquerystate-querystatepayload)
- [`MotorSequence`](#msgidmotorsequence-motorsequencepayload)
- [`KinematicsRequest`](#msgidkinematicsrequest-kinematicsrequestpayload)
- [`KinematicsData`](#msgidkinematicsdata-kinematicspayload)
- [`PowerRequest`](#msgidpowerrequest-powerrequestpayload)
- [`PowerData`](#msgidpowerdata-powerpayload)
- [`PhysicsTick`](#msgidphysicstick-physicstickpayload)
- [`StateChange`](#msgidstatechange-statechangepayload)

Each section corresponds to one `MsgId` enumerator. The **direction badge** shows which side initiates the message.

### `MsgId::Log` (`LogPayload`)

> Unidirectional log/trace message. Emitted by any component at any time; Python receives these passively from the bus.

**Direction:** `Outbound`<br>
**Publishes:** `LogService`, `MainPublisher`<br>
**Wire size:** 257 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>text</td>
      <td>char[255]</td>
      <td>bytes</td>
      <td>255</td>
      <td>0</td>
    </tr>
    <tr>
      <td>severity</td>
      <td>Severity</td>
      <td>Severity</td>
      <td>1</td>
      <td>255</td>
    </tr>
    <tr>
      <td>component</td>
      <td>ComponentId</td>
      <td>ComponentId</td>
      <td>1</td>
      <td>256</td>
    </tr>
  </tbody>
</table>

### `MsgId::QueryState` (`QueryStatePayload`)

> Carries the current SystemState. Python sends this as a request (with any state value); the simulator responds with the actual state.

**Direction:** `Bidirectional`<br>
**Publishes:** `StateService`<br>
**Subscribes:** `StateService`<br>
**Wire size:** 1 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>state</td>
      <td>SystemState</td>
      <td>SystemState</td>
      <td>1</td>
      <td>0</td>
    </tr>
  </tbody>
</table>

### `MsgId::MotorSequence` (`MotorSequencePayload`)

> Deliver a sequence of up to 10 timed motor sub-commands to the simulator. The simulator executes steps[0..num_steps-1] in real time; a new command preempts any currently running sequence.

**Direction:** `Inbound`<br>
**Subscribes:** `MotorService`<br>
**Wire size:** 65 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>cmd_id</td>
      <td>uint32_t</td>
      <td>int</td>
      <td>4</td>
      <td>0</td>
    </tr>
    <tr>
      <td>num_steps</td>
      <td>uint8_t</td>
      <td>int</td>
      <td>1</td>
      <td>4</td>
    </tr>
    <tr>
      <td>steps</td>
      <td>MotorSubCmd[10]</td>
      <td>bytes</td>
      <td>60</td>
      <td>5</td>
    </tr>
  </tbody>
</table>

#### Sub-struct: `MotorSubCmd`

> One timed motor command step, embedded in MotorSequencePayload.

**Wire size:** 6 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>speed_rpm</td>
      <td>int16_t</td>
      <td>int</td>
      <td>2</td>
      <td>0</td>
    </tr>
    <tr>
      <td>duration_us</td>
      <td>uint32_t</td>
      <td>int</td>
      <td>4</td>
      <td>2</td>
    </tr>
  </tbody>
</table>

### `MsgId::KinematicsRequest` (`KinematicsRequestPayload`)

> One-byte sentinel. Send to request a KinematicsData snapshot. The payload value is ignored.

**Direction:** `Inbound`<br>
**Subscribes:** `KinematicsService`<br>
**Wire size:** 1 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>reserved</td>
      <td>uint8_t</td>
      <td>int</td>
      <td>1</td>
      <td>0</td>
    </tr>
  </tbody>
</table>

### `MsgId::KinematicsData` (`KinematicsPayload`)

> Kinematics snapshot sent in response to a KinematicsRequest. Reflects physics state integrated since the start of the current sequence.

**Direction:** `Outbound`<br>
**Publishes:** `KinematicsService`<br>
**Wire size:** 16 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>cmd_id</td>
      <td>uint32_t</td>
      <td>int</td>
      <td>4</td>
      <td>0</td>
    </tr>
    <tr>
      <td>elapsed_us</td>
      <td>uint32_t</td>
      <td>int</td>
      <td>4</td>
      <td>4</td>
    </tr>
    <tr>
      <td>position_m</td>
      <td>float</td>
      <td>float</td>
      <td>4</td>
      <td>8</td>
    </tr>
    <tr>
      <td>speed_mps</td>
      <td>float</td>
      <td>float</td>
      <td>4</td>
      <td>12</td>
    </tr>
  </tbody>
</table>

### `MsgId::PowerRequest` (`PowerRequestPayload`)

> One-byte sentinel. Send to request a PowerData snapshot. The payload value is ignored.

**Direction:** `Inbound`<br>
**Subscribes:** `PowerService`<br>
**Wire size:** 1 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>reserved</td>
      <td>uint8_t</td>
      <td>int</td>
      <td>1</td>
      <td>0</td>
    </tr>
  </tbody>
</table>

### `MsgId::PowerData` (`PowerPayload`)

> Power-model snapshot sent in response to a PowerRequest. Models a simple battery with internal resistance drain.

**Direction:** `Outbound`<br>
**Publishes:** `PowerService`<br>
**Wire size:** 13 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>cmd_id</td>
      <td>uint32_t</td>
      <td>int</td>
      <td>4</td>
      <td>0</td>
    </tr>
    <tr>
      <td>voltage_v</td>
      <td>float</td>
      <td>float</td>
      <td>4</td>
      <td>4</td>
    </tr>
    <tr>
      <td>current_a</td>
      <td>float</td>
      <td>float</td>
      <td>4</td>
      <td>8</td>
    </tr>
    <tr>
      <td>state_of_charge</td>
      <td>uint8_t</td>
      <td>int</td>
      <td>1</td>
      <td>12</td>
    </tr>
  </tbody>
</table>

### `MsgId::PhysicsTick` (`PhysicsTickPayload`)

> Internal IPC: Broadcast at 100Hz during sequence execution to drive kinematics and power integration.

**Direction:** `Internal`<br>
**Publishes:** `MotorService`<br>
**Subscribes:** `KinematicsService`, `PowerService`, `LogService`<br>
**Wire size:** 10 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>cmd_id</td>
      <td>uint32_t</td>
      <td>int</td>
      <td>4</td>
      <td>0</td>
    </tr>
    <tr>
      <td>speed_rpm</td>
      <td>int16_t</td>
      <td>int</td>
      <td>2</td>
      <td>4</td>
    </tr>
    <tr>
      <td>dt_us</td>
      <td>uint32_t</td>
      <td>int</td>
      <td>4</td>
      <td>6</td>
    </tr>
  </tbody>
</table>

### `MsgId::StateChange` (`StateChangePayload`)

> Internal IPC: Broadcast when moving into or out of Executing state.

**Direction:** `Internal`<br>
**Publishes:** `MotorService`<br>
**Subscribes:** `KinematicsService`, `PowerService`, `StateService`, `LogService`<br>
**Wire size:** 5 bytes

<table>
  <thead>
    <tr><th>Field</th><th>C++ Type</th><th>Py Type</th><th>Bytes</th><th>Offset</th></tr>
  </thead>
  <tbody>
    <tr>
      <td>state</td>
      <td>SystemState</td>
      <td>SystemState</td>
      <td>1</td>
      <td>0</td>
    </tr>
    <tr>
      <td>cmd_id</td>
      <td>uint32_t</td>
      <td>int</td>
      <td>4</td>
      <td>1</td>
    </tr>
  </tbody>
</table>

---

## Regenerating This File

```bash
# From the repo root:
cmake -B build -G Ninja
cmake --build build --target generate_docs
# Output: build/doc/ipc_protocol.md
```

_Generated with GCC trunk `-std=c++26 -freflection` (P2996R13 + P3394R4)._
