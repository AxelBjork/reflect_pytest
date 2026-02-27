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

The service is designed for high-performance SIL verification. All core maneuvers are optimized to allow comprehensive test coverage of navigation, adaptation, and telemetry features with minimal wall-clock time overhead.
