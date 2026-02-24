#pragma once
// messages.h — Message ID enum and all payload structs.
//
// Wire format rules:
//   • Every payload must be trivially copyable and fixed-size.
//   • sizeof(received bytes) != sizeof(Payload) → message discarded.
//   • Byte order is host-native throughout.
//
// Adding a new message: add MsgId, add Payload struct, add MessageTraits<> in traits.h.

#include <array>
#include <cstdint>

// doc_annotations.h — Compile-time annotation types for reflection-based
// documentation generation (P3394 / C++26 Annotations for Reflection).
//
// Usage (on any struct or enum):
//
//   struct
//   DOC_DESC("Human readable description.")
//   MyPayload { ... };
//
// Annotation types MUST be structural. In C++26, structural types can have
// constexpr constructors as long as all members are public and they don't have
// user-defined copy/move/destroy operations.

#if defined(REFLECT_DOCS)
#define DOC_DESC(...) [[= doc::Desc(__VA_ARGS__)]]
#else
#define DOC_DESC(...)
#endif

namespace doc {

// Human-readable description of a struct or enum.
struct Desc {
  char text[512]{};
  constexpr Desc(const char* t) {
    int i = 0;
    while (t[i] != '\0' && i < 511) {
      text[i] = t[i];
      i++;
    }
    text[i] = '\0';
  }
};

}  // namespace doc

namespace ipc {

// ── Message identifiers ───────────────────────────────────────────────────────

enum class DOC_DESC("Top-level message type selector. The uint16_t wire value is the "
                    "first two bytes of every UDP datagram.") MsgId : uint16_t {
  Log,                 // unidirectional log/trace from any component
  StateRequest,        // Python -> C++: query simulator state (1-byte request)
  StateData,           // C++ -> Python: simulator state snapshot
  MotorSequence,       // Python -> C++: execute a timed command sequence
  KinematicsRequest,   // Python -> C++: query position/speed (1-byte request)
  KinematicsData,      // C++ -> Python: kinematics snapshot response
  PowerRequest,        // Python -> C++: query power state (1-byte request)
  PowerData,           // C++ -> Python: power snapshot response
  ThermalRequest,      // Python -> C++: query thermal state (1-byte request)
  ThermalData,         // C++ -> Python: thermal snapshot response
  EnvironmentAck,      // C++ -> Python: confirm receipt of environment data
  EnvironmentRequest,  // Python -> C++: query active environment state (1-byte request)
  EnvironmentData,     // Python -> C++: delivery of environmental conditions
  AutoDriveCommand,    // Python -> C++: complex autonomous driving route
  AutoDriveStatus,     // C++ -> Python: execution status and efficiency metrics
  MotorStatus,         // Motor -> Others: periodic RPM and activity update

  // -- Internal Component IPC --
  PhysicsTick,   // Motor -> Others: 100Hz tick with RPM and dt_us
  ResetRequest,  // Python -> C++: Reset all physics state

  InternalEnvRequest,  // Internal: Request pointer to environment data
  InternalEnvData,     // Internal: Shared pointer to environment data
};

// ── Supporting enums ─────────────────────────────────────────────────────────

enum class DOC_DESC("Severity level attached to every LogPayload message.") Severity : uint8_t {
  Debug,
  Info,
  Warn,
  Error
};

enum class DOC_DESC("Coarse lifecycle state of the SIL simulator.") SystemState : uint8_t {
  Init,
  Ready,
  Executing,
  Fault
};

// ── Payload structs ───────────────────────────────────────────────────────────

#pragma pack(push, 1)

// Helper sub-struct embedded in MotorSequencePayload (not a top-level message).
struct DOC_DESC("One timed motor command step, embedded in MotorSequencePayload.") MotorSubCmd {
  int16_t speed_rpm;
  uint32_t duration_us;
};

// A sequence of up to 10 timed motor sub-commands.
// The simulator executes steps[0..num_steps-1] in order, in real time.
static constexpr uint8_t kMaxSubCmds = 5;

template <std::size_t N>
struct DOC_DESC("Deliver a sequence of up to 10 timed motor sub-commands to the simulator. "
                "The simulator executes steps[0..num_steps-1] in real time; a new command "
                "preempts any currently running sequence.") MotorSequencePayloadTemplate {
  uint32_t cmd_id;
  uint8_t num_steps;
  std::array<MotorSubCmd, N> steps;
};

using MotorSequencePayload = MotorSequencePayloadTemplate<kMaxSubCmds>;

struct DOC_DESC("Unidirectional log/trace message. Emitted by any component at any time; "
                "Python receives these passively from the bus.") LogPayload {
  char text[255];
  Severity severity;
  char component[32];
};

// 1-byte sentinel requests (trigger a C++ → UDP response on the paired MsgId).
struct DOC_DESC("One-byte sentinel. Send to request a full physics reset.") ResetRequestPayload {
  uint8_t reserved;
};

struct DOC_DESC(
    "One-byte sentinel. Send to request a StateData snapshot. The payload value is ignored.")
    StateRequestPayload {
  uint8_t reserved;
};

struct DOC_DESC("State machine snapshot. "
                "Carries the current coarse lifecycle SystemState.") StatePayload {
  SystemState state;
};
struct DOC_DESC(
    "One-byte sentinel. Send to request a KinematicsData snapshot. The payload value is ignored.")
    KinematicsRequestPayload {
  uint8_t reserved;
};

struct DOC_DESC(
    "One-byte sentinel. Send to request a PowerData snapshot. The payload value is ignored.")
    PowerRequestPayload {
  uint8_t reserved;
};

struct DOC_DESC(
    "One-byte sentinel. Send to request a ThermalData snapshot. The payload value is ignored.")
    ThermalRequestPayload {
  uint8_t reserved;
};

// --- Basic Types ---
struct DOC_DESC("A 2D coordinate.") Point2D {
  float x;
  float y;
};

struct DOC_DESC("An axis-aligned 2D bounding box.") BoundingBox2D {
  Point2D min_pt;
  Point2D max_pt;
};

struct DOC_DESC("A 3D coordinate vector.") Vector3 {
  float x;
  float y;
  float z;
};

struct DOC_DESC("Environment ID Wrapper.") EnvId {
  uint32_t id;
};

struct DOC_DESC("Request conditions for a specific location.") EnvironmentRequestPayload {
  Point2D target_location;
};

// Kinematics snapshot — position integrated from sequence start.
struct DOC_DESC("Kinematics snapshot sent in response to a KinematicsRequest. "
                "Reflects physics state integrated since the start of the current sequence.")
    KinematicsPayload {
  uint32_t cmd_id;
  uint32_t elapsed_us;
  float position_m;
  float speed_mps;
};

// Power model snapshot.
struct DOC_DESC("Power-model snapshot sent in response to a PowerRequest. "
                "Models a simple battery with internal resistance drain.") PowerPayload {
  uint32_t cmd_id;
  float voltage_v;
  float current_a;
  uint8_t state_of_charge;
};

// Thermal model snapshot.
struct DOC_DESC("Thermal snapshot sent in response to a ThermalRequest. "
                "Models temperature of motor and battery based on power metrics.") ThermalPayload {
  float motor_temp_c;
  float battery_temp_c;
};

// Environment ACK.
struct DOC_DESC("ACK sent by the application when it accepts new environment data.")
    EnvironmentAckPayload {
  uint32_t region_id;
};

// Environment snapshot (inbound).
struct DOC_DESC("Environmental conditions delivered to the application from the outside world.")
    EnvironmentPayload {
  uint32_t region_id;
  BoundingBox2D bounds;  // 2D plane where these conditions are valid
  float ambient_temp_c;
  float incline_percent;
  float surface_friction;
};

// --- AutoDrive Structs ---
enum class DOC_DESC("Control strategy for the autonomous service.") DriveMode : uint8_t {
  Eco,
  Performance,
  ManualTuning
};

struct DOC_DESC("A single target maneuver point.") ManeuverNode {
  Point2D target_pos;
  int16_t speed_limit_rpm;
  uint16_t timeout_ms;
};

template <std::size_t MaxNodes>
struct DOC_DESC("High level autonomous driving route and configuration.") AutoDriveCommandTemplate {
  char route_name[32];
  DriveMode mode;
  float p_gain;  // Only used if mode == ManualTuning
  bool use_environment_tuning;
  std::array<Vector3, 3> route_transform;
  uint8_t num_nodes;
  std::array<ManeuverNode, MaxNodes> route;
};

using AutoDriveCommandPayload = AutoDriveCommandTemplate<8>;

struct DOC_DESC("Efficiency metrics for a single traveled node.") ManeuverStats {
  float initial_energy_mj;
  float final_energy_mj;
  float energy_per_meter_mj;
  float total_energy_used_mj;
};

template <std::size_t MaxNodes, std::size_t MaxEnvs>
struct DOC_DESC("Status and efficiency report from the AutonomousService.")
    AutoDriveStatusTemplate {
  uint32_t cmd_id;
  uint8_t current_node_idx;
  bool route_complete;

  // Stats per node
  uint8_t num_stats;
  std::array<ManeuverStats, MaxNodes> node_stats;

  // Environment tracking
  uint8_t num_environments_used;
  std::array<EnvId, MaxEnvs> environment_ids;
};

using AutoDriveStatusPayload = AutoDriveStatusTemplate<8, 4>;

// ── Internal Component IPC ───────────────────────────────────────────────────

struct DOC_DESC("Internal IPC: Broadcast at 100Hz during sequence execution "
                "to drive kinematics and power integration.") PhysicsTickPayload {
  uint32_t cmd_id;
  int16_t speed_rpm;
  uint32_t dt_us;
};

struct DOC_DESC("Internal IPC: Periodic RPM and activity update from MotorService.")
    MotorStatusPayload {
  uint32_t cmd_id;
  int16_t speed_rpm;
  bool is_active;
};

#pragma pack(pop)

}  // namespace ipc
