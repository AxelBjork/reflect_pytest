#pragma once
// ipc/message_bus.hpp — AF_UNIX-backed in-process pub/sub bus.

#include <any>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "socket_bus.h"

namespace ipc {

class MessageBus {
 public:
  using Handler = std::function<void(RawMessage)>;

  explicit MessageBus(std::string socket_path);
  ~MessageBus();

  MessageBus(const MessageBus&) = delete;
  MessageBus& operator=(const MessageBus&) = delete;

  // Subscribe to a specific message type.
  void subscribe(MsgId id, Handler h);

  // Publish a typed message through the bus.
  template <MsgId Id>
  void publish(const typename MessageTraits<Id>::Payload& payload) {
    if constexpr (id_has_local_trait<Id>()) {
      publish_local<Id>(payload);
    } else {
      client_.send<Id>(payload);
    }
  }

  // Local-only publish for in-process sharing (bypass socket).
  template <MsgId Id>
  void publish_local(const typename MessageTraits<Id>::Payload& payload) {
    std::vector<std::function<void(const std::any&)>> handlers;
    {
      std::lock_guard lk(mu_);
      handlers = local_handlers_[static_cast<uint16_t>(Id)];
    }
    std::any a = payload;
    for (auto& h : handlers) h(a);
  }

  // Local-only subscribe for in-process sharing.
  template <MsgId Id>
  void subscribe_local(std::function<void(const typename MessageTraits<Id>::Payload&)> h) {
    std::lock_guard lk(mu_);
    local_handlers_[static_cast<uint16_t>(Id)].push_back([h](const std::any& data) {
      h(std::any_cast<const typename MessageTraits<Id>::Payload&>(data));
    });
  }

  // Raw publish — used when the caller only has opaque bytes (e.g. UDP bridge).
  void publish_raw(MsgId id, const void* data, size_t size) {
    client_.send_raw(id, data, size);
  }

  // Allow TypedPublisher to access private methods if needed,
  // but better to just provide access to what it needs.
  template <typename T>
  friend class TypedPublisher;

 private:
  SocketBus server_;  // bound; the listener reads from here
  SocketBus client_;  // connected; publish() writes here
  int wake_[2];       // shutdown pipe

  std::thread listener_thread_;
  std::mutex mu_;
  std::unordered_map<uint16_t, std::vector<Handler>> per_id_subs_;
  std::unordered_map<uint16_t, std::vector<std::function<void(const std::any&)>>> local_handlers_;

  void dispatch(RawMessage msg);
  void listener_loop();

  template <MsgId Id>
  static constexpr bool id_has_local_trait() {
    if constexpr (requires { MessageTraits<Id>::local_only; }) {
      return MessageTraits<Id>::local_only;
    }
    return false;
  }
};

}  // namespace ipc
