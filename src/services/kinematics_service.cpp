#include "services/kinematics_service.h"

namespace sil {

KinematicsService::KinematicsService(ipc::MessageBus& bus) : bus_(bus), logger_("kinematics") {
  ipc::bind_subscriptions(bus_, this);
}

void KinematicsService::on_message(const PhysicsTickPayload& tick) {
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

void KinematicsService::on_message(const MotorStatusPayload& ms) {
  std::lock_guard lk{mu_};
  if (ms.is_active && ms.cmd_id != cmd_id_) {
    // New sequence detected
    cmd_id_ = ms.cmd_id;
    elapsed_us_ = 0;
  } else if (!ms.is_active) {
    speed_mps_ = 0.0f;
  }
}

void KinematicsService::on_message(const KinematicsRequestPayload&) {
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

}  // namespace sil
