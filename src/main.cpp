// sil_app entry point.
//
// Threads:
//   main         — waits on g_running (futex); zero CPU until signal
//   heartbeat    — publishes "Hello World #N" every 500 ms; wakes on shutdown
//   sim-exec     — inside MotorService: steps through MotorSubCmds in real time
//   sim-log      — inside LogService: logs status every 1 000 ms
//   bus-listener — inside MessageBus: AF_UNIX recv → dispatch
//   logger       — dedicated: prints every LogMessage
//   bridge-rx    — inside UdpBridge: UDP recv → bus inject

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <thread>

#include "app_components.h"
#include "message_bus.h"

static std::atomic<bool> g_running{true};

static void shutdown(int) {
  g_running.store(false, std::memory_order_release);
  g_running.notify_all();
}

static constexpr char kSockPath[] = "/tmp/sil_bus.sock";
static constexpr uint16_t kUdpPort = 9000;

int main() {
  std::signal(SIGTERM, shutdown);
  std::signal(SIGINT, shutdown);

  ipc::MessageBus bus(kSockPath);

  // create_app_services now owns the LogService
  auto services = sil::create_app_services(bus);
  auto& log_service = std::get<sil::LogService>(services.services);

  std::printf("[sil_app] started (UDP bridge on :%u, bus on %s)\n", kUdpPort, kSockPath);
  std::fflush(stdout);

  // ── Heartbeat thread ──────────────────────────────────────────────────────
  std::mutex hb_mu;
  std::condition_variable hb_cv;

  std::thread heartbeat([&] {
    uint32_t n = 0;
    while (true) {
      {
        std::unique_lock lk{hb_mu};
        hb_cv.wait_for(lk, std::chrono::milliseconds(500),
                       [&] { return !g_running.load(std::memory_order_acquire); });
      }
      if (!g_running.load(std::memory_order_acquire)) break;
      LogPayload p{};
      std::snprintf(p.text, sizeof(p.text), "[heartbeat] TICK %u", ++n);
      p.severity = Severity::Info;
      std::strncpy(p.component, "main", sizeof(p.component) - 1);
      log_service.log(p);
    }
  });

  // ── Main thread: futex wait ───────────────────────────────────────────────
  g_running.wait(true);

  hb_cv.notify_all();
  heartbeat.join();

  std::printf("[sil_app] shutting down\n");
  return 0;
}
