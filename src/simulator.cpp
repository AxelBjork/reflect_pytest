// simulator.cpp — Real-time motor sequence simulator.
//
// Physics constants (round numbers for easy pytest assertions):
//   speed_mps  = speed_rpm * 0.01        (100 RPM → 1.0 m/s)
//   current_a  = |speed_rpm| * 0.005     (100 RPM → 0.5 A)
//   voltage_v -= current * 0.5 * dt_s    (drains ~0.25V/s at 100 RPM)
//   soc        = (voltage - 10.5) / (12.6 - 10.5) * 100

#include "simulator.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "traits.h"

namespace sil {

static constexpr uint32_t kTickUs = 10'000;  // physics tick = 10 ms
static constexpr uint32_t kLogMs = 1'000;    // status log period

Simulator::Simulator(ipc::MessageBus& bus) : bus_(bus) {
  using namespace ipc;

  // ── QueryState — report simulator state ────────────────────────────────────
  bus_.subscribe(MsgId::QueryState, [this, &bus](RawMessage msg) {
    if (msg.payload.size() != sizeof(QueryStatePayload)) return;
    QueryStatePayload r{};
    {
      std::lock_guard lk{mu_};
      r.state = state_.sys_state;
    }
    bus.publish<MsgId::QueryState>(r);
  });

  // ── MotorSequence — queue a new command ────────────────────────────────────
  bus_.subscribe(MsgId::MotorSequence, [this](RawMessage msg) {
    if (msg.payload.size() != sizeof(MotorSequencePayload)) return;
    MotorSequencePayload cmd{};
    std::memcpy(&cmd, msg.payload.data(), sizeof(cmd));
    cmd.num_steps = std::min(cmd.num_steps, kMaxSubCmds);
    {
      std::lock_guard lk{mu_};
      state_.pending_cmd = cmd;
      state_.have_cmd = true;
    }
    cv_.notify_one();
  });

  // ── KinematicsRequest — snapshot query ────────────────────────────────────
  bus_.subscribe(MsgId::KinematicsRequest, [this, &bus](RawMessage msg) {
    KinematicsPayload k{};
    {
      std::lock_guard lk{mu_};
      k.cmd_id = state_.cmd_id;
      k.elapsed_us = state_.elapsed_us;
      k.position_m = state_.position_m;
      k.speed_mps = state_.speed_mps;
    }
    bus.publish<MsgId::KinematicsData>(k);
  });

  // ── PowerRequest — snapshot query ─────────────────────────────────────────
  bus_.subscribe(MsgId::PowerRequest, [this, &bus](RawMessage msg) {
    PowerPayload p{};
    {
      std::lock_guard lk{mu_};
      p.cmd_id = state_.cmd_id;
      p.voltage_v = state_.voltage_v;
      p.current_a = state_.current_a;
      p.state_of_charge = state_.soc;
    }
    bus.publish<MsgId::PowerData>(p);
  });

  exec_thread_ = std::thread([this] { exec_loop(); });
  log_thread_ = std::thread([this] { log_loop(); });
}

Simulator::~Simulator() {
  {
    std::lock_guard lk{mu_};
    running_ = false;
    state_.have_cmd = true;  // unblock exec_loop wait
  }
  cv_.notify_all();
  if (exec_thread_.joinable()) exec_thread_.join();
  if (log_thread_.joinable()) log_thread_.join();
}

// ── Physics tick ─────────────────────────────────────────────────────────────

void Simulator::tick_physics(int16_t speed_rpm, uint32_t dt_us) {
  const float dt_s = static_cast<float>(dt_us) / 1e6f;
  state_.speed_mps = speed_rpm * K_RPM_TO_MPS;
  state_.position_m += state_.speed_mps * dt_s;
  state_.current_a = std::abs(speed_rpm) * K_RPM_TO_AMPS;
  const float dv = state_.current_a * R_INT_OHM * dt_s;
  state_.voltage_v = std::max(V_MIN, state_.voltage_v - dv);
  state_.soc = static_cast<uint8_t>(
      std::clamp((state_.voltage_v - V_MIN) / (V_MAX - V_MIN) * 100.0f, 0.0f, 100.0f));
  state_.elapsed_us += dt_us;
}

// ── Execution thread ──────────────────────────────────────────────────────────

void Simulator::exec_loop() {
  while (running_) {
    // Wait for a pending command.
    ipc::MotorSequencePayload cmd{};
    {
      std::unique_lock lk{mu_};
      cv_.wait(lk, [this] { return state_.have_cmd || !running_; });
      if (!running_) break;
      cmd = state_.pending_cmd;
      state_.have_cmd = false;
      state_.cmd_id = cmd.cmd_id;
      state_.elapsed_us = 0;
      state_.sys_state = ipc::SystemState::Executing;
    }

    bool preempted = false;

    for (uint8_t i = 0; i < cmd.num_steps && running_; ++i) {
      const auto& step = cmd.steps[i];
      if (step.duration_us == 0) break;

      uint32_t remaining = step.duration_us;
      while (remaining > 0 && running_) {
        uint32_t dt = std::min(remaining, kTickUs);
        // Sleep entirely outside the lock.
        std::this_thread::sleep_for(std::chrono::microseconds(dt));

        std::lock_guard sg{mu_};
        if (state_.have_cmd) {
          preempted = true;
          break;
        }
        tick_physics(step.speed_rpm, dt);
        remaining -= dt;
      }
      if (preempted) break;
    }

    if (!preempted && running_) {
      std::lock_guard sg{mu_};
      state_.speed_mps = 0.0f;
      state_.current_a = 0.0f;
      state_.sys_state = ipc::SystemState::Ready;
    }
  }
}

// ── Status log thread (1 000 ms) ─────────────────────────────────────────────

void Simulator::log_loop() {
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(kLogMs));
    if (!running_) break;
    publish_log_status();
  }
}

void Simulator::publish_log_status() {
  ipc::LogPayload p{};
  float pos, v, curr;
  ipc::SystemState st;
  uint32_t cid;
  {
    std::lock_guard lk{mu_};
    pos = state_.position_m;
    v = state_.voltage_v;
    curr = state_.current_a;
    st = state_.sys_state;
    cid = state_.cmd_id;
  }
  const char* state_str = st == ipc::SystemState::Ready       ? "Ready"
                          : st == ipc::SystemState::Executing ? "Executing"
                          : st == ipc::SystemState::Fault     ? "Fault"
                                                              : "Init";

  std::snprintf(p.text, sizeof(p.text), "[sim] state=%s cmd=%u pos=%.3fm v=%.2fV i=%.3fA",
                state_str, cid, pos, v, curr);
  p.severity = ipc::Severity::Info;
  p.component = ipc::ComponentId::Simulator;
  bus_.publish<ipc::MsgId::Log>(p);
}

}  // namespace sil
