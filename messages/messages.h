#pragma once
// messages.h — Message ID enum and all payload structs.
//
// Wire format rules:
//   • Every payload must be trivially copyable and fixed-size.
//   • sizeof(received bytes) != sizeof(Payload) → message discarded.
//   • Byte order is host-native throughout.
//
// Adding a new message: add MsgId, add Payload struct, add MessageTraits<> in traits.h.

#include <cstdint>

// doc_annotations.h — Compile-time annotation types for reflection-based
// documentation generation (P3394 / C++26 Annotations for Reflection).
//
// Usage (on any struct or enum):
//
//   struct
//   [[= doc::Desc("Human readable description.") ]]
//   MyPayload { ... };
//
// Annotation types MUST be structural. In C++26, structural types can have
// constexpr constructors as long as all members are public and they don't have
// user-defined copy/move/destroy operations.

namespace doc {

// Human-readable description of a struct or enum.
struct Desc {
  const char* text;
  constexpr Desc(const char* t) : text(t) {
  }
};

}  // namespace doc

namespace ipc {

// ── Message identifiers ───────────────────────────────────────────────────────

enum class[[= doc::Desc("Top-level message type selector. The uint16_t wire value is the "
                        "first two bytes of every UDP datagram.")]] MsgId : uint16_t {
  Log,                // unidirectional log/trace from any component
  QueryState,         // bidirectional: request + response carrying SystemState
  MotorSequence,      // Python -> C++: execute a timed command sequence
  KinematicsRequest,  // Python -> C++: query position/speed (1-byte request)
  KinematicsData,     // C++ -> Python: kinematics snapshot response
  PowerRequest,       // Python -> C++: query power state (1-byte request)
  PowerData,          // C++ -> Python: power snapshot response

  // -- Internal Component IPC --
  PhysicsTick,  // Motor -> Others: 100Hz tick with RPM and dt_us
  StateChange,  // Motor -> Others: SystemState transition
};

// ── Supporting enums ─────────────────────────────────────────────────────────

enum class[
    [= doc::Desc("Severity level attached to every LogPayload message.")]] Severity : uint8_t {
  Debug,
  Info,
  Warn,
  Error
};

enum class[
    [= doc::Desc("Identifies the subsystem that emitted a LogPayload.")]] ComponentId : uint8_t {
  Main,
  Bus,
  Logger,
  Bridge,
  Test,
  Simulator
};

enum class[[= doc::Desc("Coarse lifecycle state of the SIL simulator.")]] SystemState : uint8_t {
  Init,
  Ready,
  Executing,
  Fault
};

// ── Payload structs ───────────────────────────────────────────────────────────

#pragma pack(push, 1)

// Helper sub-struct embedded in MotorSequencePayload (not a top-level message).
struct[[= doc::Desc("One timed motor command step, embedded in MotorSequencePayload.")]]
    MotorSubCmd {
  int16_t speed_rpm;
  uint32_t duration_us;
};

// A sequence of up to 10 timed motor sub-commands.
// The simulator executes steps[0..num_steps-1] in order, in real time.
static constexpr uint8_t kMaxSubCmds = 10;

struct[[= doc::Desc("Deliver a sequence of up to 10 timed motor sub-commands to the simulator. "
                    "The simulator executes steps[0..num_steps-1] in real time; a new command "
                    "preempts any currently running sequence.")]] MotorSequencePayload {
  uint32_t cmd_id;
  uint8_t num_steps;
  MotorSubCmd steps[kMaxSubCmds];
};

struct[[= doc::Desc("Unidirectional log/trace message. Emitted by any component at any time; "
                    "Python receives these passively from the bus.")]] LogPayload {
  char text[255];
  Severity severity;
  ComponentId component;
};

struct[[= doc::Desc("Carries the current SystemState. Python sends this as a request "
                    "(with any state value); the simulator responds with the actual state.")]]
    QueryStatePayload {
  SystemState state;
};

// 1-byte sentinel requests (trigger a C++ → UDP response on the paired MsgId).
struct[[= doc::Desc(
    "One-byte sentinel. Send to request a KinematicsData snapshot. The payload value is ignored.")]]
    KinematicsRequestPayload {
  uint8_t reserved;
};

struct[[= doc::Desc(
    "One-byte sentinel. Send to request a PowerData snapshot. The payload value is ignored.")]]
    PowerRequestPayload {
  uint8_t reserved;
};

// Kinematics snapshot — position integrated from sequence start.
struct[[= doc::Desc("Kinematics snapshot sent in response to a KinematicsRequest. "
                    "Reflects physics state integrated since the start of the current sequence.")]]
    KinematicsPayload {
  uint32_t cmd_id;
  uint32_t elapsed_us;
  float position_m;
  float speed_mps;
};

// Power model snapshot.
struct[[= doc::Desc("Power-model snapshot sent in response to a PowerRequest. "
                    "Models a simple battery with internal resistance drain.")]] PowerPayload {
  uint32_t cmd_id;
  float voltage_v;
  float current_a;
  uint8_t state_of_charge;
};

// ── Internal Component IPC ───────────────────────────────────────────────────

struct[[= doc::Desc("Internal IPC: Broadcast at 100Hz during sequence execution "
                    "to drive kinematics and power integration.")]] PhysicsTickPayload {
  uint32_t cmd_id;
  int16_t speed_rpm;
  uint32_t dt_us;
};

struct[[= doc::Desc("Internal IPC: Broadcast when moving into or out of Executing state.")]]
    StateChangePayload {
  SystemState state;
  uint32_t cmd_id;
};

#pragma pack(pop)

}  // namespace ipc
