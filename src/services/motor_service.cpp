#include "services/motor_service.h"

#include <algorithm>

namespace sil {

MotorService::MotorService(ipc::MessageBus& bus) : bus_(bus), logger_("motor") {

}

void MotorService::on_message(const MotorSequencePayload& cmd) {
  logger_.info("Received MotorSequence with %u steps", cmd.num_steps);
  std::lock_guard lk{mu_};
  current_cmd_ = cmd;
  current_cmd_.num_steps = std::min(current_cmd_.num_steps, kMaxSubCmds);
  for (uint8_t i = 0; i < current_cmd_.num_steps; ++i) {
    current_cmd_.steps[i].speed_rpm = std::clamp(
        current_cmd_.steps[i].speed_rpm, static_cast<int16_t>(-6000), static_cast<int16_t>(6000));
  }
  current_step_idx_ = 0;
  bool start_immediately = (current_cmd_.num_steps > 0 && current_cmd_.steps[0].duration_us > 0);

  if (start_immediately) {
    active_ = true;
    step_remaining_us_ = current_cmd_.steps[0].duration_us;
    bus_.publish<MsgId::MotorStatus>({current_cmd_.cmd_id, current_cmd_.steps[0].speed_rpm, true});
  } else {
    active_ = false;
    bus_.publish<MsgId::MotorStatus>({current_cmd_.cmd_id, 0, false});
    logger_.info("Motor sequence %u stopped immediately (zero duration or empty)",
                 current_cmd_.cmd_id);
  }
}

void MotorService::on_message(const PhysicsTickPayload& tick) {
  std::lock_guard lk{mu_};
  if (!active_ || tick.cmd_id != current_cmd_.cmd_id) return;

  if (tick.dt_us >= step_remaining_us_) {
    advance_step();
  } else {
    step_remaining_us_ -= tick.dt_us;
  }
}

void MotorService::advance_step() {
  current_step_idx_++;
  if (current_step_idx_ < current_cmd_.num_steps) {
    const auto& step = current_cmd_.steps[current_step_idx_];
    step_remaining_us_ = step.duration_us;
    bus_.publish<MsgId::MotorStatus>({current_cmd_.cmd_id, step.speed_rpm, true});
    logger_.info("Advancing to step %u: %d RPM", current_step_idx_, step.speed_rpm);
  } else {
    active_ = false;
    bus_.publish<MsgId::MotorStatus>({current_cmd_.cmd_id, 0, false});
    logger_.info("Motor sequence %u complete", current_cmd_.cmd_id);
  }
}

}  // namespace sil
