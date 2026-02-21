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

// ── Test: single MotorSequence round-trip ────────────────────────────────────

TEST(SocketBus, MotorCmdRoundTrip) {
  MotorSequencePayload sent{};
  sent.cmd_id = 1;
  sent.num_steps = 1;
  sent.steps[0] = {.speed_rpm = 1500, .duration_us = 500'000};
  std::optional<RawMessage> received;

  std::thread server([&] {
    SocketBus bus(kSockPath, /*create=*/true);
    received = bus.recv();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  {
    SocketBus client(kSockPath, /*create=*/false);
    client.send<MsgId::MotorSequence>(sent);
  }

  server.join();

  ASSERT_TRUE(received.has_value());
  EXPECT_TRUE((payload_matches<MsgId::MotorSequence>(*received, sent)));
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
      KinematicsRequestPayload p{.reserved = static_cast<uint8_t>(i)};
      client.send<MsgId::KinematicsRequest>(p);
    }
  }

  server.join();

  ASSERT_EQ(received.size(), size_t(kCount));
  for (const auto& msg : received) EXPECT_EQ(msg.msgId, MsgId::KinematicsRequest);
}
