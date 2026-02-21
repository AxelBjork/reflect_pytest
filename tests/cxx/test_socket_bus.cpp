// Two-thread smoke test for the AF_UNIX socket bus.
// One thread acts as server (binding the socket), the other as client (connecting).

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <optional>
#include <thread>

#include "socket_bus.h"
#include "traits.h"

using namespace ipc;

static constexpr char kSockPath[] = "/tmp/test_socket_bus.sock";

// ── Helper: validate a RawMessage carries the expected typed payload ───────────

template <MsgId Id>
static bool payload_matches(const RawMessage& msg,
                            const typename MessageTraits<Id>::Payload& expected) {
  using P = typename MessageTraits<Id>::Payload;
  if (msg.msgId != Id) return false;
  if (msg.payload.size() != sizeof(P)) return false;
  P actual{};
  std::memcpy(&actual, msg.payload.data(), sizeof(P));
  return std::memcmp(&actual, &expected, sizeof(P)) == 0;
}

// ── Test: single MotorCmd round-trip ─────────────────────────────────────────

TEST(SocketBus, MotorCmdRoundTrip) {
  MotorCmdPayload sent{.speed_rpm = 1500, .direction = 0, .enabled = 1};
  std::optional<RawMessage> received;

  // Server thread: bind the socket, receive one message.
  std::thread server([&] {
    SocketBus bus(kSockPath, /*create=*/true);
    received = bus.recv();
  });

  // Give server time to bind before the client connects.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Client thread: connect and send.
  {
    SocketBus client(kSockPath, /*create=*/false);
    client.send<MsgId::MotorCmd>(sent);
  }

  server.join();

  ASSERT_TRUE(received.has_value());
  EXPECT_TRUE((payload_matches<MsgId::MotorCmd>(*received, sent)));
}

// ── Test: multiple messages in sequence ──────────────────────────────────────

TEST(SocketBus, MultipleMsgsSequential) {
  constexpr int kCount = 5;
  std::vector<RawMessage> received;

  std::thread server([&] {
    SocketBus bus(kSockPath, /*create=*/true);
    for (int i = 0; i < kCount; ++i) {
      auto msg = bus.recv();
      ASSERT_TRUE(msg.has_value());
      received.push_back(std::move(*msg));
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  {
    SocketBus client(kSockPath, /*create=*/false);
    for (int i = 0; i < kCount; ++i) {
      SensorDataPayload p{
          .temperature = float(20 + i), .pressure = 101.3f, .timestamp_ms = uint32_t(i * 100)};
      client.send<MsgId::SensorData>(p);
    }
  }

  server.join();

  ASSERT_EQ(received.size(), size_t(kCount));
  for (const auto& msg : received) EXPECT_EQ(msg.msgId, MsgId::SensorData);
}
