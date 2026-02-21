// sil_app entry point.
//
// Threads:
//   main         — sends "Hello World" LogMessage every 500 ms
//   bus-listener — inside MessageBus: AF_UNIX recv → dispatch
//   logger        — dedicated: prints every LogMessage
//   bridge-rx     — inside UdpBridge: UDP recv → bus inject

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <thread>

#include "ipc/message_bus.hpp"
#include "ipc/udp_bridge.hpp"
#include "logger.hpp"

static std::atomic<bool> g_running{true};

static constexpr char kSockPath[] = "/tmp/sil_bus.sock";
static constexpr uint16_t kUdpPort = 9000;

int main() {
  std::signal(SIGTERM, [](int) { g_running = false; });
  std::signal(SIGINT, [](int) { g_running = false; });

  ipc::MessageBus bus(kSockPath);

  // ── Logger thread ─────────────────────────────────────────────────────────
  auto logger = sil::create_logger(bus);

  // ── QueryState Handler ────────────────────────────────────────────────────
  bus.subscribe(ipc::MsgId::QueryState, [&bus](ipc::RawMessage msg) {
    if (msg.payload.size() != sizeof(ipc::QueryStatePayload)) return;

    // As soon as this message is received by the client we just echo it back mapped to
    // the system state! The SIL is alive and well.
    ipc::QueryStatePayload response{};
    response.state = ipc::SystemState::Ready;
    bus.publish<ipc::MsgId::QueryState>(response);
  });

  // ── UDP bridge ────────────────────────────────────────────────────────────
  ipc::UdpBridge bridge(bus, kUdpPort);

  std::printf("[sil_app] started (UDP bridge on :%u, bus on %s)\n", kUdpPort, kSockPath);
  std::fflush(stdout);

  // ── Hello World loop ──────────────────────────────────────────────────────
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (!g_running) break;

    ipc::LogPayload p{};
    std::strncpy(p.text, "Hello World", sizeof(p.text) - 1);
    p.severity = ipc::Severity::Info;
    p.component = ipc::ComponentId::Main;
    bus.publish<ipc::MsgId::Log>(p);
  }

  // ── Cleanup ───────────────────────────────────────────────────────────────
  logger.reset();  // Destroy background loop first

  std::printf("[sil_app] shutting down\n");
  return 0;
}
