#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "component.h"
#include "message_bus.h"
#include "messages.h"

namespace sil {

class DOC_DESC(
    "Asynchronous logging sink.\n\n"
    "Collects LogPayloads into a queue and processes them on a background thread "
    "to avoid blocking simulation services.") LogService {
 public:
  using Subscribes = ipc::MsgList<>;
  using Publishes = ipc::MsgList<ipc::MsgId::Log>;

  explicit LogService(ipc::MessageBus& bus);
  ~LogService();

  LogService(const LogService&) = delete;
  LogService& operator=(const LogService&) = delete;

  // Thread-safe async log entry point
  void log(const ipc::LogPayload& p);

 private:
  ipc::MessageBus& bus_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::thread worker_thread_;
  bool running_{true};
  std::queue<ipc::LogPayload> queue_;

  void worker_loop();
};

}  // namespace sil
