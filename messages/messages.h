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
  MotorCmd = 0x0001,
  SensorData = 0x0002,
  Log = 0x0003,
  QueryState = 0x0004,
};

// ── Supporting enums ─────────────────────────────────────────────────────────

enum class Severity : uint8_t { Debug = 0, Info, Warn, Error };
enum class ComponentId : uint8_t { Main = 0, Bus, Logger, Bridge, Test };
enum class SystemState : uint8_t { Init = 0, Ready, Fault };

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

static_assert(sizeof(MotorCmdPayload) == 4);
static_assert(sizeof(SensorDataPayload) == 12);
static_assert(sizeof(LogPayload) == 257);
static_assert(sizeof(QueryStatePayload) == 1);

}  // namespace ipc
