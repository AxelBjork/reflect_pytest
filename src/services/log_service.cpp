#include "services/log_service.h"

#include <cstdio>
#include <cstring>

#include "component_logger.h"

namespace sil {

static constexpr const char* sev_str(Severity s) {
  switch (s) {
    case Severity::Debug:
      return "DEBUG";
    case Severity::Info:
      return "INFO";
    case Severity::Warn:
      return "WARN";
    case Severity::Error:
      return "ERROR";
  }
  return "?";
}

LogService::LogService(ipc::MessageBus& bus) : bus_(bus) {
  ComponentLogger::init(*this);
  ipc::bind_subscriptions(bus_, this);
  worker_thread_ = std::thread([this] { worker_loop(); });
}

LogService::~LogService() {
  {
    std::lock_guard lk{mu_};
    running_ = false;
  }
  cv_.notify_all();
  if (worker_thread_.joinable()) worker_thread_.join();
}

void LogService::log(const LogPayload& p) {
  {
    std::lock_guard lk{mu_};
    queue_.push(p);
  }
  cv_.notify_one();
}

void LogService::worker_loop() {
  while (true) {
    std::queue<::LogPayload> to_process;
    {
      std::unique_lock lk{mu_};
      cv_.wait(lk, [this] { return !queue_.empty() || !running_; });
      if (!running_ && queue_.empty()) break;
      std::swap(to_process, queue_);
    }

    while (!to_process.empty()) {
      auto p = to_process.front();
      to_process.pop();

      // 1. Print to console (this is why we are on a worker thread)
      std::printf("[%s][%s] %s\n", sev_str(p.severity), p.component, p.text);
      std::fflush(stdout);

      // 2. Publish to message bus so UdpBridge can send it to Python
      bus_.publish<MsgId::Log>(p);
    }
  }
}

}  // namespace sil
