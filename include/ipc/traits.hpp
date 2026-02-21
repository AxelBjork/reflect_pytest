#pragma once
// ipc/traits.hpp — Maps every MsgId enumerator to its payload type.
//
// Adding a new message:
//   1. Add an enumerator to MsgId in messages.hpp.
//   2. Add a payload struct in messages.hpp.
//   3. Add a MessageTraits<> specialisation here.
//
// The base template is intentionally left undefined so missing specialisations
// produce a clear linker error rather than silent misbehaviour.

#include "messages.hpp"
#include <string_view>

namespace ipc {

template <MsgId Id>
struct MessageTraits; // undefined — every MsgId *must* be specialised

template <>
struct MessageTraits<MsgId::MotorCmd> {
    using Payload = MotorCmdPayload;
    static constexpr std::string_view name = "MotorCmd";
};

template <>
struct MessageTraits<MsgId::SensorData> {
    using Payload = SensorDataPayload;
    static constexpr std::string_view name = "SensorData";
};

template <>
struct MessageTraits<MsgId::Log> {
    using Payload = LogPayload;
    static constexpr std::string_view name = "Log";
};

} // namespace ipc
