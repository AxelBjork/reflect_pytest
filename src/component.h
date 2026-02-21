#pragma once
// component.h — Traits for compile-time component pub/sub wiring.

#include <cstring>

#include "message_bus.h"

namespace ipc {

// A type-level list of message IDs.
template <MsgId... Ids>
struct MsgList {};

namespace detail {

template <typename Component, MsgId Id>
void bind_one(MessageBus& bus, Component* c) {
  bus.subscribe(Id, [c](RawMessage msg) {
    using Payload = typename MessageTraits<Id>::Payload;
    if (msg.payload.size() != sizeof(Payload)) return;
    Payload p{};
    std::memcpy(&p, msg.payload.data(), sizeof(p));
    c->on_message(p);
  });
}

template <typename Component, MsgId... Ids>
void bind_all(MessageBus& bus, Component* c, MsgList<Ids...>) {
  (bind_one<Component, Ids>(bus, c), ...);
}

}  // namespace detail

// Automatically wire all MsgIds in Component::Subscribes to c->on_message(payload).
template <typename Component>
void bind_subscriptions(MessageBus& bus, Component* c) {
  detail::bind_all(bus, c, typename Component::Subscribes{});
}

}  // namespace ipc
