// sil_app entry point.
//
// Threads:
//   main         — sends "Hello World" LogMessage every 500 ms
//   bus-listener — inside MessageBus: AF_UNIX recv → dispatch
//   logger        — dedicated: prints every LogMessage
//   bridge-rx     — inside UdpBridge: UDP recv → bus inject

#include "ipc/message_bus.hpp"
#include "ipc/udp_bridge.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>

static std::atomic<bool> g_running{true};

static constexpr char kSockPath[] = "/tmp/sil_bus.sock";
static constexpr uint16_t kUdpPort = 9000;

// ── Severity / Component to string helpers
// ────────────────────────────────────

static constexpr const char *sev_str(ipc::Severity s) {
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

static constexpr const char *comp_str(ipc::ComponentId c) {
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

int main() {
  std::signal(SIGTERM, [](int) { g_running = false; });
  std::signal(SIGINT, [](int) { g_running = false; });

  ipc::MessageBus bus(kSockPath);

  // ── Logger thread ─────────────────────────────────────────────────────────
  std::queue<ipc::LogPayload> log_queue;
  std::mutex log_mu;
  std::condition_variable log_cv;
  std::atomic<bool> log_running{true};

  bus.subscribe(ipc::MsgId::Log, [&](ipc::RawMessage msg) {
    if (msg.payload.size() != sizeof(ipc::LogPayload))
      return;
    ipc::LogPayload p{};
    std::memcpy(&p, msg.payload.data(), sizeof(p));
    {
      std::lock_guard lk(log_mu);
      log_queue.push(p);
    }
    log_cv.notify_one();
  });

  std::thread logger([&] {
    while (log_running) {
      std::unique_lock lk(log_mu);
      log_cv.wait(lk, [&] { return !log_queue.empty() || !log_running; });
      while (!log_queue.empty()) {
        auto p = log_queue.front();
        log_queue.pop();
        lk.unlock();
        char text[256]{};
        std::strncpy(text, p.text, 255);
        std::printf("[%s][%s] %s\n", sev_str(p.severity), comp_str(p.component),
                    text);
        std::fflush(stdout);
        lk.lock();
      }
    }
  });

  // ── UDP bridge ────────────────────────────────────────────────────────────
  ipc::UdpBridge bridge(bus, kUdpPort);

  std::printf("[sil_app] started (UDP bridge on :%u, bus on %s)\n", kUdpPort,
              kSockPath);
  std::fflush(stdout);

  // ── Hello World loop ──────────────────────────────────────────────────────
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (!g_running)
      break;

    ipc::LogPayload p{};
    std::strncpy(p.text, "Hello World", sizeof(p.text) - 1);
    p.severity = ipc::Severity::Info;
    p.component = ipc::ComponentId::Main;
    bus.publish<ipc::MsgId::Log>(p);
  }

  // ── Cleanup ───────────────────────────────────────────────────────────────
  log_running = false;
  log_cv.notify_all();
  logger.join();

  std::printf("[sil_app] shutting down\n");
  return 0;
}
