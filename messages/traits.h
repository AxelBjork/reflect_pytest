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
struct MessageTraits<MsgId::QueryState> {
  using Payload = QueryStatePayload;
  static constexpr std::string_view name = "QueryState";
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

}  // namespace ipc
