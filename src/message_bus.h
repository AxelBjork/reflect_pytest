#pragma once
// ipc/message_bus.h — Pure in-process pub/sub message dispatcher.
//
// All dispatch is lock-free. Subscriptions must be performed during
// the initialization phase (e.g. in service constructors) before any
// threads start publishing. The start of any thread that publishes
// serves as a memory barrier for the handler map.

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "msg_base.h"

namespace ipc {

class MessageBus {
 public:
  MessageBus() = default;
  ~MessageBus() = default;

  MessageBus(const MessageBus&) = delete;
  MessageBus& operator=(const MessageBus&) = delete;

  // Subscribe to a specific message type.
  // Must be called before any threads start publishing.
  template <MsgId Id>
  void subscribe(std::function<void(const typename MessageTraits<Id>::Payload&)> h) {
    handlers_[static_cast<uint16_t>(Id)].push_back([h](const void* data) {
      h(*static_cast<const typename MessageTraits<Id>::Payload*>(data));
    });
  }

  // Publish a typed message through the bus (lock-free).
  template <MsgId Id>
  void publish(const typename MessageTraits<Id>::Payload& payload) {
    dispatch(Id, &payload);
  }

  // Allow TypedPublisher to access private methods if needed.
  template <typename T>
  friend class TypedPublisher;

 private:
  std::unordered_map<uint16_t, std::vector<std::function<void(const void*)>>> handlers_;

  void dispatch(MsgId id, const void* payload);
};

}  // namespace ipc
