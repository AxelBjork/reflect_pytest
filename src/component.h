#pragma once
// component.h — Traits for compile-time component pub/sub wiring.

#include <cstring>
#include <type_traits>

#include "message_bus.h"

namespace ipc {

// A type-level list of message IDs.
template <MsgId... Ids>
struct MsgList {};

namespace detail {

template <MsgId Target, MsgId... Ids>
struct MsgContains;

template <MsgId Target>
struct MsgContains<Target> : std::false_type {};

template <MsgId Target, MsgId First, MsgId... Rest>
struct MsgContains<Target, First, Rest...>
    : std::conditional_t<Target == First, std::true_type, MsgContains<Target, Rest...>> {};

template <MsgId Target, MsgId... Ids>
constexpr bool msg_in_list_v = MsgContains<Target, Ids...>::value;

template <MsgId Target, typename List>
struct IsInList;

template <MsgId Target, MsgId... Ids>
struct IsInList<Target, MsgList<Ids...>> : MsgContains<Target, Ids...> {};

template <MsgId Target, typename List>
constexpr bool is_in_list_v = IsInList<Target, List>::value;

template <typename Component, MsgId Id>
void bind_one(MessageBus& bus, Component* c) {
  bus.subscribe<Id>([c](const typename MessageTraits<Id>::Payload& p) { c->on_message(p); });
}

template <typename Component, MsgId... Ids>
void bind_all(MessageBus& bus, Component* c, MsgList<Ids...>) {
  (bind_one<Component, Ids>(bus, c), ...);
}

// Try to publish from raw bytes by matching the MsgId against the Publishes
// list.  Returns true if the id was found and the size matched.
template <MsgId... Ids>
bool try_publish_raw(MessageBus& bus, MsgId id, const void* data, size_t size, MsgList<Ids...>) {
  return ((id == Ids && size == sizeof(typename MessageTraits<Ids>::Payload) &&
           [&] {
             typename MessageTraits<Ids>::Payload p{};
             std::memcpy(&p, data, sizeof(p));
             bus.publish<Ids>(p);
             return true;
           }()) ||
          ...);
}

}  // namespace detail

// A wrapper around MessageBus that enforces Component::Publishes.
template <typename Component>
class TypedPublisher {
 public:
  explicit TypedPublisher(MessageBus& bus) : bus_(bus) {
  }

  template <MsgId Id>
  void publish(const typename MessageTraits<Id>::Payload& payload) {
    static_assert(detail::is_in_list_v<Id, typename Component::Publishes>,
                  "Message ID not found in Component::Publishes list!");
    bus_.publish<Id>(payload);
  }

  // Runtime-checked publish for cases where the MsgId and payload size are
  // determined at runtime (e.g. from a network packet). Returns true if
  // authorized and published.
  bool publish_if_authorized(MsgId id, const void* data, size_t size) {
    return detail::try_publish_raw(bus_, id, data, size, typename Component::Publishes{});
  }

  // Access the underlying bus for subscriptions.
  MessageBus& bus() {
    return bus_;
  }

 private:
  MessageBus& bus_;
};

// Automatically wire all MsgIds in Component::Subscribes to c->on_message(payload).
template <typename Component>
void bind_subscriptions(MessageBus& bus, Component* c) {
  detail::bind_all(bus, c, typename Component::Subscribes{});
}

}  // namespace ipc
