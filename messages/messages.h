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

namespace ipc {

// ── Message identifiers ───────────────────────────────────────────────────────

enum class MsgId : uint16_t {
  Log,
  QueryState,
  MotorSequence,      // Python → C++: execute a timed command sequence
  KinematicsRequest,  // Python → C++: query position/speed (1-byte request)
  KinematicsData,     // C++ → Python: kinematics snapshot response
  PowerRequest,       // Python → C++: query power state (1-byte request)
  PowerData,          // C++ → Python: power snapshot response
};

// ── Supporting enums ─────────────────────────────────────────────────────────

enum class Severity : uint8_t { Debug, Info, Warn, Error };
enum class ComponentId : uint8_t { Main, Bus, Logger, Bridge, Test, Simulator };
enum class SystemState : uint8_t { Init, Ready, Executing, Fault };

// ── Payload structs ───────────────────────────────────────────────────────────

#pragma pack(push, 1)

// Helper sub-struct embedded in MotorSequencePayload (not a top-level message).
struct MotorSubCmd {
  int16_t speed_rpm;     // signed; negative = reverse; 0 = idle/coast
  uint32_t duration_us;  // hold time in microseconds; 0 = end-of-sequence sentinel
};

// A sequence of up to 10 timed motor sub-commands.
// The simulator executes steps[0..num_steps-1] in order, in real time.
static constexpr uint8_t kMaxSubCmds = 10;
struct MotorSequencePayload {
  uint32_t cmd_id;                 // caller-supplied ID echoed in telemetry
  uint8_t num_steps;               // number of valid entries in steps[]
  MotorSubCmd steps[kMaxSubCmds];  // fixed-size array; entries beyond num_steps ignored
};

struct LogPayload {
  char text[255];  // null-terminated
  Severity severity;
  ComponentId component;
};

struct QueryStatePayload {
  SystemState state;
};

// 1-byte sentinel requests (trigger a C++ → UDP response on the paired MsgId).
struct KinematicsRequestPayload {
  uint8_t reserved;
};
struct PowerRequestPayload {
  uint8_t reserved;
};

// Kinematics snapshot — position integrated from sequence start.
struct KinematicsPayload {
  uint32_t cmd_id;      // which sequence this belongs to (0 if idle)
  uint32_t elapsed_us;  // microseconds since sequence start (total for last seq if idle)
  float position_m;     // metres from origin (positive = forward)
  float speed_mps;      // current speed (0 when idle)
};

// Power model snapshot.
struct PowerPayload {
  uint32_t cmd_id;
  float voltage_v;          // pack voltage; starts at V_MAX, drains with load
  float current_a;          // load current; 0 when idle
  uint8_t state_of_charge;  // 0–100 %
};

#pragma pack(pop)

}  // namespace ipc
