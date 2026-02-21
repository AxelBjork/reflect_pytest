#pragma once
// ipc/messages.hpp — Message ID enum and all payload structs.
//
// Rules:
//   • Every payload must be trivially copyable and fixed-size.
//   • If sizeof(received bytes) != sizeof(Payload), the message is discarded.
//   • Byte order is host-native throughout (C++ writes, Python reads same bytes).

#include <cstdint>

namespace ipc {

// ── Message identifiers ───────────────────────────────────────────────────────

enum class MsgId : uint16_t {
  MotorCmd,
  SensorData,
  Log,
  QueryState,
};

// ── Supporting enums ─────────────────────────────────────────────────────────

enum class Severity : uint8_t { Debug, Info, Warn, Error };
enum class ComponentId : uint8_t { Main, Bus, Logger, Bridge, Test };
enum class SystemState : uint8_t { Init, Ready, Fault };

// ── Payload structs ───────────────────────────────────────────────────────────
// All fields are plain arithmetic types so the structs are trivially copyable
// and have no implicit padding surprises (add static_asserts below).

#pragma pack(push, 1)

struct MotorCmdPayload {
  int16_t speed_rpm;
  uint8_t direction;  // 0 = forward, 1 = reverse
  uint8_t enabled;    // 0 = off, 1 = on
};

struct SensorDataPayload {
  float temperature;  // °C
  float pressure;     // kPa
  uint32_t timestamp_ms;
};

struct LogPayload {
  char text[255];         // null-terminated message
  Severity severity;      // 1 byte
  ComponentId component;  // 1 byte
};

struct QueryStatePayload {
  SystemState state;
};

#pragma pack(pop)

}  // namespace ipc
