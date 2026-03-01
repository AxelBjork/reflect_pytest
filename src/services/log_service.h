#pragma once
#include <condition_variable>
#include <mutex>
#include "component_logger.h"
#include <queue>
#include <thread>

#include "core_msgs.h"
#include "publisher.h"
#include "msg_base.h"

namespace sil {

class DOC_DESC(
    "Asynchronous logging sink.\n\n"
    "Collects LogPayloads into a queue and processes them on a background thread "
    "to avoid blocking simulation services.") LogService {
 public:
  using Subscribes = ipc::MsgList<>;
  using Publishes = ipc::MsgList<MsgId::Log>;

  explicit LogService(ipc::MessageBus& bus);
  ~LogService();

  void start();

  LogService(const LogService&) = delete;
  LogService& operator=(const LogService&) = delete;

  // Thread-safe async log entry point
  void log(const LogPayload& p);

 private:
  ipc::TypedPublisher<LogService> bus_;
  ComponentLogger logger_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::thread worker_thread_;
  bool running_{true};
  std::queue<::LogPayload> queue_;

  void worker_loop();
};

}  // namespace sil
