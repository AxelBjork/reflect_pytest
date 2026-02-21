#pragma once
#include <mutex>

#include "component.h"

namespace sil {

inline constexpr float K_RPM_TO_MPS = 0.01f;

class DOC_DESC(
    "Simulates vehicle motion by integrating motor RPM over time to track position and linear "
    "velocity.\n\n"
    "The physics model applies a linear conversion from RPM to meters-per-second, and integrates "
    "this velocity over the `PhysicsTick` delta-time to continuously evaluate the vehicle's "
    "position:\n\n"
    "$$ v = \\text{RPM} \\times 0.01 \\text{ (m/s)} $$\n\n"
    "$$ x = \\int v \\, dt $$")
 KinematicsService {
 public:
  using Subscribes =
      ipc::MsgList<ipc::MsgId::PhysicsTick, ipc::MsgId::KinematicsRequest, ipc::MsgId::StateChange>;
  using Publishes = ipc::MsgList<ipc::MsgId::KinematicsData>;

  explicit KinematicsService(ipc::MessageBus& bus) : bus_(bus) {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const ipc::PhysicsTickPayload& tick) {
    std::lock_guard lk{mu_};
    float dt_s = tick.dt_us / 1e6f;
    speed_mps_ = tick.speed_rpm * K_RPM_TO_MPS;
    position_m_ += speed_mps_ * dt_s;
    elapsed_us_ += tick.dt_us;
  }

  void on_message(const ipc::StateChangePayload& sc) {
    std::lock_guard lk{mu_};
    cmd_id_ = sc.cmd_id;
    if (sc.state == ipc::SystemState::Executing) {
      elapsed_us_ = 0;
    } else if (sc.state == ipc::SystemState::Ready) {
      speed_mps_ = 0.0f;
    }
  }

  void on_message(const ipc::KinematicsRequestPayload&) {
    ipc::KinematicsPayload p{};
    {
      std::lock_guard lk{mu_};
      p.cmd_id = cmd_id_;
      p.elapsed_us = elapsed_us_;
      p.position_m = position_m_;
      p.speed_mps = speed_mps_;
    }
    bus_.publish<ipc::MsgId::KinematicsData>(p);
  }

 private:
  ipc::MessageBus& bus_;
  std::mutex mu_;

  uint32_t cmd_id_{0};
  uint32_t elapsed_us_{0};
  float position_m_{0.0f};
  float speed_mps_{0.0f};
};

}  // namespace sil
