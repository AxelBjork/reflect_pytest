#pragma once
// ipc/message_bus.h — Pure in-process pub/sub message dispatcher.
//
// All dispatch is lock-free. Subscriptions must be performed during
// the initialization phase (e.g. in service constructors) before any
// threads start publishing. The start of any thread that publishes
// serves as a memory barrier for the handler map.

#include <cstdint>

#include "msg_base.h"
#include "publisher.h"

namespace ipc {

class MessageBus {
 public:
  using DispatchFn = void (*)(void* ctx, MsgId id, const void* payload);

  explicit MessageBus(void* ctx, DispatchFn dispatcher) : ctx_(ctx), dispatcher_(dispatcher) {
  }
  ~MessageBus() = default;

  MessageBus(const MessageBus&) = delete;
  MessageBus& operator=(const MessageBus&) = delete;

  // Publish a typed message through the bus (lock-free).
  template <MsgId Id>
  void publish(const typename MessageTraits<Id>::Payload& payload) {
    dispatch(Id, &payload);
  }

  // Allow TypedPublisher to access private methods.
  template <typename T>
  friend class TypedPublisher;

 private:
  void* const ctx_;
  const DispatchFn dispatcher_;

  void dispatch(MsgId id, const void* payload);
};

namespace detail {

template <MsgId... Ids>
bool try_publish_raw(MessageBus& bus, MsgId id, const void* data, size_t size, MsgList<Ids...>) {
  return ((id == Ids && size == sizeof(typename MessageTraits<Ids>::Payload) &&
           [&] {
             static_assert(std::is_trivially_copyable_v<typename MessageTraits<Ids>::Payload>);
             typename MessageTraits<Ids>::Payload p;
             std::memcpy(&p, data, sizeof(p));
             bus.publish<Ids>(p);
             return true;
           }()) ||
          ...);
}

}  // namespace detail

template <typename Component>
template <MsgId Id>
void TypedPublisher<Component>::publish(const typename MessageTraits<Id>::Payload& payload) {
  static_assert(detail::is_in_list_v<Id, typename Component::Publishes>,
                "Message ID not found in Component::Publishes list!");
  bus_.publish<Id>(payload);
}

}  // namespace ipc
