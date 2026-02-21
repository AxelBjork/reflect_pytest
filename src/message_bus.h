#pragma once
// ipc/message_bus.hpp — AF_UNIX-backed in-process pub/sub bus.

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "socket_bus.h"  // brings in traits.hpp → messages.hpp

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

  // Subscribe to every message (used by UdpBridge).
  void subscribe_all(Handler h);

  // Publish a typed message through the bus.
  template <MsgId Id>
  void publish(const typename MessageTraits<Id>::Payload& payload) {
    client_.send<Id>(payload);
  }

  // Raw publish — used when the caller only has opaque bytes (e.g. UDP bridge).
  void publish_raw(MsgId id, const void* data, size_t size) {
    client_.send_raw(id, data, size);
  }

 private:
  SocketBus server_;  // bound; the listener reads from here
  SocketBus client_;  // connected; publish() writes here
  int wake_[2];       // shutdown pipe

  std::thread listener_thread_;
  std::mutex mu_;
  std::unordered_map<uint16_t, std::vector<Handler>> per_id_subs_;
  std::vector<Handler> wildcard_subs_;

  void listener_loop();
  void dispatch(RawMessage msg);
};

}  // namespace ipc
