#pragma once
// traits.h — Maps every MsgId enumerator to its payload type.
//
// The base template is intentionally left undefined so missing specialisations
// produce a clear compile error rather than silent misbehaviour.

#include <string_view>

#include "messages.h"

namespace ipc {

template <MsgId Id>
struct MessageTraits;

template <>
struct MessageTraits<MsgId::Log> {
  using Payload = LogPayload;
  static constexpr std::string_view name = "Log";
};
template <>
struct MessageTraits<MsgId::StateRequest> {
  using Payload = StateRequestPayload;
  static constexpr std::string_view name = "StateRequest";
};
template <>
struct MessageTraits<MsgId::StateData> {
  using Payload = StatePayload;
  static constexpr std::string_view name = "StateData";
};
template <>
struct MessageTraits<MsgId::MotorSequence> {
  using Payload = MotorSequencePayload;
  static constexpr std::string_view name = "MotorSequence";
};
template <>
struct MessageTraits<MsgId::KinematicsRequest> {
  using Payload = KinematicsRequestPayload;
  static constexpr std::string_view name = "KinematicsRequest";
};
template <>
struct MessageTraits<MsgId::KinematicsData> {
  using Payload = KinematicsPayload;
  static constexpr std::string_view name = "KinematicsData";
};
template <>
struct MessageTraits<MsgId::PowerRequest> {
  using Payload = PowerRequestPayload;
  static constexpr std::string_view name = "PowerRequest";
};
template <>
struct MessageTraits<MsgId::PowerData> {
  using Payload = PowerPayload;
  static constexpr std::string_view name = "PowerData";
};

template <>
struct MessageTraits<MsgId::ThermalRequest> {
  using Payload = ThermalRequestPayload;
  static constexpr std::string_view name = "ThermalRequest";
};
template <>
struct MessageTraits<MsgId::ThermalData> {
  using Payload = ThermalPayload;
  static constexpr std::string_view name = "ThermalData";
};

template <>
struct MessageTraits<MsgId::EnvironmentCommand> {
  using Payload = EnvironmentCommandPayload;
  static constexpr std::string_view name = "EnvironmentCommand";
};
template <>
struct MessageTraits<MsgId::EnvironmentRequest> {
  using Payload = EnvironmentRequestPayload;
  static constexpr std::string_view name = "EnvironmentRequest";
};
template <>
struct MessageTraits<MsgId::EnvironmentData> {
  using Payload = EnvironmentPayload;
  static constexpr std::string_view name = "EnvironmentData";
};

template <>
struct MessageTraits<MsgId::PhysicsTick> {
  using Payload = PhysicsTickPayload;
  static constexpr std::string_view name = "PhysicsTick";
};

template <>
struct MessageTraits<MsgId::StateChange> {
  using Payload = StateChangePayload;
  static constexpr std::string_view name = "StateChange";
};

template <>
struct MessageTraits<MsgId::ResetRequest> {
  using Payload = ResetRequestPayload;
  static constexpr std::string_view name = "ResetRequest";
};

}  // namespace ipc
