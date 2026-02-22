# Mermaid Prototypes

## Option 6: Independent Network Routing Subgraph

Based on your feedback, we'll extract the UDP Client and socket routing out of the Pytest logical grouping, and put it into an independent third Subgraph serving as the Linux local interface `127.0.0.1`.

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
        TX(["TX Socket<br/>PORT 9000<br/>(sendto)"])
        RX(["RX Socket<br/>PORT 9001<br/>(recvfrom)"])
    end

    %% ─── COLUMN 3: C++ SUT ───
    subgraph sim["Simulator"]
        MotorService(["MotorService<br/><br/>Manages the thread that executes<br/>timed motor commands in real-time."])
        KinematicsService(["KinematicsService<br/><br/>Simulates vehicle motion by integrating<br/>motor RPM over time to track position<br/>and linear velocity."])
        PowerService(["PowerService<br/><br/>Models a simple battery pack<br/>dynamically responding to motor load."])
        StateService(["StateService<br/><br/>Maintains the central lifecycle state<br/>machine of the simulation."])
        LogService(["LogService<br/><br/>Periodically aggregates system state<br/>into human-readable text logs for debugging."])
        MainPublisher(["MainPublisher"])
        UdpBridge(["UdpBridge<br/><br/>Stateful bridge that relays IPC messages<br/>between the internal MessageBus and<br/>external UDP clients."])
        
        %% Pure Internal Messages
        MotorService -.->|PhysicsTick,<br/>StateChange| KinematicsService
        MotorService -.->|PhysicsTick,<br/>StateChange| PowerService
        MotorService -.->|StateChange| StateService
        MotorService -.->|PhysicsTick,<br/>StateChange| LogService
        
        %% Bridge Distribution
        UdpBridge -->|MotorSequence| MotorService
        UdpBridge -->|KinematicsRequest| KinematicsService
        UdpBridge -->|PowerRequest| PowerService
        UdpBridge -->|QueryState| StateService
        
        KinematicsService -->|KinematicsData| UdpBridge
        PowerService -->|PowerData| UdpBridge
        StateService -->|QueryState| UdpBridge
        MainPublisher -->|Log| UdpBridge
        LogService -->|Log| UdpBridge
    end

    %% ─── INBOUND PATH ───
    TestCase -->|send_msg| TX
    TX -->|"MotorSequence<br/>KinematicsRequest<br/>PowerRequest<br/>QueryState"| UdpBridge

    %% ─── OUTBOUND PATH ───
    UdpBridge -->|"KinematicsData<br/>PowerData<br/>QueryState<br/>Log"| RX
    RX -->|recv_msg| TestCase
```
