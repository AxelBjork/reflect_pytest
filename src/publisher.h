#pragma once
// publisher.h — Lightweight handle for component-side publishing.

#include <cstring>
#include <type_traits>

#include "msg_base.h"

namespace ipc {

class MessageBus;

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

template <MsgId Target, typename List>
struct IsInList;

template <MsgId Target, MsgId... Ids>
struct IsInList<Target, MsgList<Ids...>> : MsgContains<Target, Ids...> {};

template <MsgId Target, typename List>
constexpr bool is_in_list_v = IsInList<Target, List>::value;

// Forward declaration of the raw publish mechanism.
template <MsgId... Ids>
bool try_publish_raw(MessageBus& bus, MsgId id, const void* data, size_t size, MsgList<Ids...>);

}  // namespace detail

// A wrapper around MessageBus that enforces Component::Publishes.
// Services should include this header to define their publisher members.
template <typename Component>
class TypedPublisher {
 public:
  explicit TypedPublisher(MessageBus& bus) : bus_(bus) {
  }

  template <MsgId Id>
  void publish(const typename MessageTraits<Id>::Payload& payload);

  bool publish_if_authorized(MsgId id, const void* data, size_t size) {
    return detail::try_publish_raw(bus_, id, data, size, typename Component::Publishes{});
  }

  MessageBus& bus() {
    return bus_;
  }

 private:
  MessageBus& bus_;
};

}  // namespace ipc
