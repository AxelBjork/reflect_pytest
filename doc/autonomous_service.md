# Autonomous Service Design Specification

The `AutonomousService` is a high-level vehicle controller modeled as a stateful agent that manages mission-level routing, environmental adaptation, and efficiency telemetry. It operates within the SIL (Software-In-the-Loop) environment, building upon the services provided by the motor, kinematics, and power subsystems.

## 1. Architectural Overview

The service acts as an orchestrator that translates goal-oriented routes into low-level execution sequences. It leverage C++26 static reflection to ensure that its internal state and external interface remain synchronized with the overall system protocol.

### 1.1 Messaging and Integration
The service participates in the internal message bus to consume physical feedback (position, speed, energy) and environmental conditions. It is specifically designed to work with the **strictly inbound** environment model, where environmental data is provided by external simulation components and merely consumed by the application logic.

For a detailed reference of the wire-level messages consumed and produced by this service, see the [IPC Protocol Reference](file:///workspaces/reflect_pytest/doc/README.md).

## 2. Core Operational Capabilities

### 2.1 Proportional Trajectory Execution
The service manages routes defined by coordinate-based maneuver points. It implements a trajectory following logic that:
- Periodically polls the kinematics state.
- Adjusts motor propulsion sequences to reach goal targets while respecting segment speed limits.
- Manages the lifecycle of the route, transitioning through Init, Ready, Executing, and Complete states.

### 2.2 Spatial Environment Adaptation
To maintain simulation fidelity across varied terrains, the service monitors environmental context:
- **Spatial Validation**: Tracks if the vehicle's current position is covered by active environmental data (temperature, incline, friction).
- **Proactive Acquisition**: Publishes requests for new data when transitioning into unknown spatial regions.
- **Transactional Application**: Ensures that new environmental factors are acknowledged before they affect the underlying physical models.

### 2.3 Efficiency and Telemetry Tracking
A key objective of the service is to provide high-fidelity reporting of mission efficiency:
- **Energy Accounting**: Integrates power consumption across maneuvers to report total energy used in Millijoules.
- **Distance-normalized Efficiency**: Calculates energy-per-meter metrics for comparative analysis.
- **Environmental Context**: Logs the specific environmental regions encountered, providing a verifiable audit trail for efficiency variances.

## 3. Implementation and Performance

## 4. Current Status and Gap Analysis (As of Feb 2026)

The following assessment evaluates the proof-of-concept against the core architectural design:

### 4.1 Design Compliance Matrix
| Principle | Status | Observation |
| :--- | :--- | :--- |
| **Reflection-Driven** | ✅ | Fully utilizes P2996/P3394 reflection for IPC. |
| **SOA & Isolation** | ✅ | Strictly decoupled via `MessageBus`. |
| **Asynchronous IPC** | ✅ | Non-blocking communication model. |
| **Request-Response** | ✅ | Implements poll-based kinematics/power updates. |
| **Timed Sequences** | ⚠️ | Only supports single-step motor commands currently. |

### 4.2 Identified Gaps
- **Physics Consistency**: Energy integration uses a hardcoded `0.01s` timestep rather than the `dt_us` from the `PhysicsTickPayload`.
- **Closed-Loop Control**: The `p_gain` parameter from route commands is currently ignored; motor speed is set directly via open-loop RPM limits.
- **IPC Mapping**: Relies on internal `InternalEnvRequest` which may require explicit bridging to external `EnvironmentRequest` listeners.
- **Dimensionality**: Logic is currently restricted to 1D (X-axis) despite message support for 2D/3D.

## 5. Future Roadmap

1. **Precision Integration**: Refactor physics integration to use dynamic `dt` from the simulation clock.
2. **Proportional Navigation**: Implement a P-controller (using the `p_gain` field) for smoother arrivals at target nodes.
3. **Formal State Machine**: Transition from internal booleans to a shared `SystemState` enum for better observability across the bus.
4. **Multi-dimensional Support**: Extend trajectory logic to handle 2D (X, Y) maneuvers and coordinate transforms.
