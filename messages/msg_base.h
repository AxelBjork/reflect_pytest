#pragma once
#include <cstdint>

// C++26 Reflection Annotations (P3394R4)
// See doc/reflection_cheat_sheet.md for details on the [[= ]] syntax.
#if defined(REFLECT_DOCS)
#define DOC_DESC(...) [[= doc::Desc(__VA_ARGS__)]]
#else
#define DOC_DESC(...)
#endif

namespace doc {
struct Desc {
  char text[1024]{};
  constexpr Desc(const char* t) {
    int i = 0;
    while (t[i] != '\0' && i < 1023) {
      text[i] = t[i];
      i++;
    }
    text[i] = '\0';
  }
};
}  // namespace doc

enum class Severity : uint8_t { Debug = 0, Info = 1, Warn = 2, Error = 3 };
enum class SystemState : uint8_t { Init = 0, Ready = 1, Executing = 2, Stopping = 3, Fault = 4 };

/**
 * Global message identifier.
 * Moved from ipc:: namespace to simplify cross-service communication.
 */
enum class DOC_DESC("Top-level message type selector. The uint16_t wire value is the "
                    "first two bytes of every UDP datagram.") MsgId : uint16_t {
  // Core messages
  Log = 0,
  PhysicsTick = 1,
  StateRequest = 2,
  StateData = 3,

  // Simulation/Vehicle messages
  MotorSequence = 10,
  MotorStatus = 11,
  KinematicsRequest = 20,
  KinematicsData = 21,
  PowerRequest = 30,
  PowerData = 31,
  ThermalRequest = 40,
  ThermalData = 41,

  // Autonomous/Environment messages
  EnvironmentAck = 50,
  EnvironmentRequest = 51,
  EnvironmentData = 52,
  AutoDriveCommand = 60,
  AutoDriveStatus = 61,

  // Internal/Service-specific (formerly in internal_messages.h)
  InternalEnvRequest = 1000,
  InternalEnvData = 1001,

  // SensorService messages
  SensorRequest = 1100,
  SensorAck = 1101,
};

/**
 * Global MessageTraits base template.
 * Specialized alongside each payload to provide metadata like name and payload type.
 */
template <MsgId Id>
struct MessageTraits;
