// sil_app entry point.
#include "message_bus.h"
//
// Threads:
//   main         — waits on g_running (futex); zero CPU until signal
//   heartbeat    — publishes heartbeat logs every 500 ms; wakes on shutdown
//   bridge-rx    — inside UdpBridge: UDP recv → bus inject
//   sim-clock    — inside StateService: 100 Hz PhysicsTick heartbeat
//   log-worker   — inside LogService: async log queue processing

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <thread>

#include "app_components.h"

static std::atomic<bool> g_running{true};

static void shutdown(int) {
  g_running.store(false, std::memory_order_release);
  g_running.notify_all();
}

static constexpr uint16_t kUdpPort = 9000;

int main() {
  std::signal(SIGTERM, shutdown);
  std::signal(SIGINT, shutdown);

  auto app = sil::create_app_services();
  auto& log_service = std::get<sil::LogService>(app.services);

  std::printf("[sil_app] started (UDP bridge on :%u)\n", kUdpPort);
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
  return 0;
}
