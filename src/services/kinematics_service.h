#pragma once
#include <mutex>

#include "component.h"
#include "component_logger.h"
#include "core_msgs.h"
#include "message_bus.h"
#include "simulation_msgs.h"

namespace sil {

inline constexpr float K_RPM_TO_MPS = 0.01f;

class DOC_DESC(
    "Simulates vehicle motion by integrating motor RPM over time to track position and linear "
    "velocity.\n\n"
    "The physics model applies a linear conversion from RPM to meters-per-second, and integrates "
    "this velocity over the `PhysicsTick` delta-time to continuously evaluate the vehicle's "
    "position:\n\n"
    "$$ v = \\text{RPM} \\times 0.01 \\text{ (m/s)} $$\n\n"
    "$$ x = \\int v \\, dt $$") KinematicsService {
 public:
  using Subscribes = ipc::MsgList<MsgId::PhysicsTick, MsgId::KinematicsRequest, MsgId::MotorStatus>;
  using Publishes = ipc::MsgList<MsgId::KinematicsData>;

  explicit KinematicsService(ipc::MessageBus& bus) : bus_(bus), logger_("kinematics") {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const PhysicsTickPayload& tick) {
    std::lock_guard lk{mu_};
    float dt_s = tick.dt_us / 1e6f;
    speed_mps_ = tick.speed_rpm * K_RPM_TO_MPS;
    position_m_ += speed_mps_ * dt_s;
    elapsed_us_ += tick.dt_us;
    cmd_id_ = tick.cmd_id;

    // Log every 100 ticks (approx 1s)
    static uint32_t count = 0;
    if (++count % 100 == 0) {
      logger_.info("Position: %.3fm, Speed: %.3fm/s", position_m_, speed_mps_);
    }
  }

  void on_message(const MotorStatusPayload& ms) {
    std::lock_guard lk{mu_};
    if (ms.is_active && ms.cmd_id != cmd_id_) {
      // New sequence detected
      cmd_id_ = ms.cmd_id;
      elapsed_us_ = 0;
    } else if (!ms.is_active) {
      speed_mps_ = 0.0f;
    }
  }

  void on_message(const KinematicsRequestPayload&) {
    KinematicsPayload p{};
    {
      std::lock_guard lk{mu_};
      p.cmd_id = cmd_id_;
      p.elapsed_us = elapsed_us_;
      p.position_m = position_m_;
      p.speed_mps = speed_mps_;
    }
    bus_.publish<MsgId::KinematicsData>(p);
  }

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  uint32_t cmd_id_{0};
  uint32_t elapsed_us_{0};
  float position_m_{0.0f};
  float speed_mps_{0.0f};
};

}  // namespace sil
