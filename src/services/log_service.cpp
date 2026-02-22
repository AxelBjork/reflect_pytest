#include "services/log_service.h"

#include <algorithm>
#include <cmath>

namespace sil {

LogService::LogService(ipc::MessageBus& bus) : bus_(bus) {
  ipc::bind_subscriptions(bus_, this);
  log_thread_ = std::thread([this] { log_loop(); });
}

LogService::~LogService() {
  {
    std::lock_guard lk{mu_};
    running_ = false;
  }
  cv_.notify_all();
  if (log_thread_.joinable()) log_thread_.join();
}

void LogService::on_message(const ipc::PhysicsTickPayload& tick) {
  std::lock_guard lk{mu_};
  float dt_s = tick.dt_us / 1e6f;
  speed_mps_ = tick.speed_rpm * 0.01f;
  pos_m_ += speed_mps_ * dt_s;
  current_a_ = std::abs(tick.speed_rpm) * 0.005f;
  float dv = current_a_ * 0.5f * dt_s;
  voltage_v_ = std::max(10.5f, voltage_v_ - dv);
}

void LogService::on_message(const ipc::StateChangePayload& sc) {
  std::lock_guard lk{mu_};
  cmd_id_ = sc.cmd_id;
  state_ = sc.state;
  if (sc.state == ipc::SystemState::Ready) {
    speed_mps_ = 0.0f;
    current_a_ = 0.0f;
  }
}

void LogService::log_loop() {
  while (true) {
    {
      std::unique_lock lk{mu_};
      cv_.wait_for(lk, std::chrono::milliseconds(1000), [this] { return !running_; });
      if (!running_) break;
    }

    ipc::SystemState st;
    uint32_t cid;
    float pos, v, curr;
    {
      std::lock_guard lk{mu_};
      st = state_;
      cid = cmd_id_;
      pos = pos_m_;
      v = voltage_v_;
      curr = current_a_;
    }

    const char* state_str = st == ipc::SystemState::Ready       ? "Ready"
                            : st == ipc::SystemState::Executing ? "Executing"
                            : st == ipc::SystemState::Fault     ? "Fault"
                                                                : "Init";

    ipc::LogPayload p{};
    std::snprintf(p.text, sizeof(p.text), "[sim] state=%s cmd=%u pos=%.3fm v=%.2fV i=%.3fA",
                  state_str, cid, pos, v, curr);
    p.severity = ipc::Severity::Info;
    p.component = ipc::ComponentId::Simulator;
    bus_.publish<ipc::MsgId::Log>(p);
  }
}

}  // namespace sil
