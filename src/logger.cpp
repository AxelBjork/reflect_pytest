// src/logger.cpp

#include "logger.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>

#include "ipc/message_bus.hpp"
#include "ipc/messages.hpp"

namespace sil {

static constexpr const char* sev_str(ipc::Severity s) {
  switch (s) {
    case ipc::Severity::Debug:
      return "DEBUG";
    case ipc::Severity::Info:
      return "INFO";
    case ipc::Severity::Warn:
      return "WARN";
    case ipc::Severity::Error:
      return "ERROR";
  }
  return "?";
}

static constexpr const char* comp_str(ipc::ComponentId c) {
  switch (c) {
    case ipc::ComponentId::Main:
      return "main";
    case ipc::ComponentId::Bus:
      return "bus";
    case ipc::ComponentId::Logger:
      return "logger";
    case ipc::ComponentId::Bridge:
      return "bridge";
    case ipc::ComponentId::Test:
      return "test";
  }
  return "?";
}

class Logger {
 public:
  Logger(ipc::MessageBus& bus) {
    bus.subscribe(ipc::MsgId::Log, [this](ipc::RawMessage msg) {
      if (msg.payload.size() != sizeof(ipc::LogPayload)) return;
      ipc::LogPayload p{};
      std::memcpy(&p, msg.payload.data(), sizeof(p));
      {
        std::lock_guard lk(mu_);
        queue_.push(p);
      }
      cv_.notify_one();
    });

    thread_ = std::thread([this] {
      while (running_) {
        std::unique_lock lk(mu_);
        cv_.wait(lk, [this] { return !queue_.empty() || !running_; });
        while (!queue_.empty()) {
          auto p = queue_.front();
          queue_.pop();
          lk.unlock();

          char text[256]{};
          std::strncpy(text, p.text, 255);
          std::printf("[%s][%s] %s\n", sev_str(p.severity), comp_str(p.component), text);
          std::fflush(stdout);

          lk.lock();
        }
      }
    });
  }

  ~Logger() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
  }

 private:
  std::queue<ipc::LogPayload> queue_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::atomic<bool> running_{true};
  std::thread thread_;
};

// Singleton storage handled by main to keep it alive
LoggerPtr create_logger(ipc::MessageBus& bus) {
  return LoggerPtr(new Logger(bus));
}

void destroy_logger(Logger* logger) {
  delete logger;
}

}  // namespace sil
