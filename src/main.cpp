// sil_app entry point.
//
// Threads:
//   main         — waits on g_running (futex); zero CPU until signal
//   heartbeat    — publishes "Hello World #N" every 500 ms; wakes on shutdown
//   bus-listener — inside MessageBus: AF_UNIX recv → dispatch
//   logger        — dedicated: prints every LogMessage
//   bridge-rx     — inside UdpBridge: UDP recv → bus inject

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <thread>

#include "logger.h"
#include "message_bus.h"
#include "udp_bridge.h"

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

  // ── Logger thread ─────────────────────────────────────────────────────────
  auto logger = sil::create_logger(bus);

  // ── QueryState Handler ────────────────────────────────────────────────────
  bus.subscribe(ipc::MsgId::QueryState, [&bus](ipc::RawMessage msg) {
    if (msg.payload.size() != sizeof(ipc::QueryStatePayload)) return;
    ipc::QueryStatePayload response{};
    response.state = ipc::SystemState::Ready;
    bus.publish<ipc::MsgId::QueryState>(response);
  });

  // ── UDP bridge ────────────────────────────────────────────────────────────
  ipc::UdpBridge bridge(bus, kUdpPort);

  std::printf("[sil_app] started (UDP bridge on :%u, bus on %s)\n", kUdpPort, kSockPath);
  std::fflush(stdout);

  // ── Heartbeat thread — prints "Hello World #N" every 500 ms ──────────────
  // Uses an interruptible wait so shutdown is immediate regardless of cadence.
  std::mutex hb_mu;
  std::condition_variable hb_cv;

  std::thread heartbeat([&] {
    uint32_t count = 0;
    while (true) {
      {
        std::unique_lock lk{hb_mu};
        hb_cv.wait_for(lk, std::chrono::milliseconds(500),
                       [&] { return !g_running.load(std::memory_order_acquire); });
      }
      if (!g_running.load(std::memory_order_acquire)) break;

      ipc::LogPayload p{};
      std::snprintf(p.text, sizeof(p.text), "Hello World #%u", ++count);
      p.severity = ipc::Severity::Info;
      p.component = ipc::ComponentId::Main;
      bus.publish<ipc::MsgId::Log>(p);
    }
  });

  // ── Main thread: block on g_running (futex, zero CPU) ────────────────────
  g_running.wait(true);

  // Wake heartbeat and join cleanly.
  hb_cv.notify_all();
  heartbeat.join();

  // ── Cleanup ───────────────────────────────────────────────────────────────
  logger.reset();

  std::printf("[sil_app] shutting down\n");
  return 0;
}
