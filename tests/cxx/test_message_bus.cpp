// Smoke test for the in-process MessageBus pub/sub dispatcher.

#include <gtest/gtest.h>

#include "core_msgs.h"
#include "message_bus.h"
#include "simulation_msgs.h"

using namespace ipc;

// ── Test: single MotorSequence round-trip ────────────────────────────────────

TEST(MessageBus, MotorCmdRoundTrip) {
  MessageBus bus;

  ::MotorSequencePayload sent{};
  sent.cmd_id = 1;
  sent.num_steps = 1;
  sent.steps[0] = {.speed_rpm = 1500, .duration_us = 500'000};

  ::MotorSequencePayload received{};
  bool got_it = false;

  bus.subscribe<MsgId::MotorSequence>([&](const ::MotorSequencePayload& p) {
    received = p;
    got_it = true;
  });

  bus.publish<MsgId::MotorSequence>(sent);

  ASSERT_TRUE(got_it);
  EXPECT_EQ(received.cmd_id, sent.cmd_id);
  EXPECT_EQ(received.num_steps, sent.num_steps);
  EXPECT_EQ(received.steps[0].speed_rpm, sent.steps[0].speed_rpm);
}

// ── Test: multiple subscribers receive the same message ─────────────────────

TEST(MessageBus, MultiSubscriberFanout) {
  MessageBus bus;

  int count_a = 0, count_b = 0;

  bus.subscribe<MsgId::KinematicsRequest>([&](const ::KinematicsRequestPayload&) { ++count_a; });
  bus.subscribe<MsgId::KinematicsRequest>([&](const ::KinematicsRequestPayload&) { ++count_b; });

  ::KinematicsRequestPayload p{.reserved = 42};
  bus.publish<MsgId::KinematicsRequest>(p);

  EXPECT_EQ(count_a, 1);
  EXPECT_EQ(count_b, 1);
}

// ── Test: unrelated subscribers are not invoked ─────────────────────────────

TEST(MessageBus, NoSideDispatch) {
  MessageBus bus;

  bool wrong_called = false;
  bus.subscribe<MsgId::PowerRequest>([&](const ::PowerRequestPayload&) { wrong_called = true; });

  ::StateRequestPayload p{.reserved = 0};
  bus.publish<MsgId::StateRequest>(p);

  EXPECT_FALSE(wrong_called);
}

// ── Test: simple dispatch check ──────────────────────────────────

TEST(MessageBus, BasicDispatch) {
  MessageBus bus;

  int count = 0;
  bus.subscribe<MsgId::StateData>([&](const ::StatePayload&) { ++count; });

  bus.publish<MsgId::StateData>(::StatePayload{.state = SystemState::Ready});
  EXPECT_EQ(count, 1);

  bus.publish<MsgId::StateData>(::StatePayload{.state = SystemState::Executing});
  EXPECT_EQ(count, 2);
}
