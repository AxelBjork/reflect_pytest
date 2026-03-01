#include "services/autonomous_service.h"

#include <algorithm>

namespace sil {

AutonomousService::AutonomousService(ipc::MessageBus& bus) : bus_(bus), logger_("auto") {

}

void AutonomousService::on_message(const AutoDriveCommandPayload& cmd) {
  std::lock_guard lk{mu_};
  logger_.info("Received AutoDriveCommand: route %s (%u nodes)", cmd.route_name, cmd.num_nodes);

  cmd_ = cmd;
  current_node_idx_ = 0;
  route_active_ = (cmd.num_nodes > 0);

  // Reset status tracking
  status_ = {};
  status_.cmd_id = 1234;  // Arbitrary sequence id for this autonomous run
  used_envs_.clear();

  current_env_ptr_ = nullptr;

  current_total_energy_j_ = 0.0f;
  node_start_energy_j_ = 0.0f;
  node_start_pos_m_ = last_pos_m_;

  if (route_active_) {
    publish_motor_sequence_for_current_node();
  }
}

void AutonomousService::on_message(const InternalEnvDataPayload& env) {
  std::lock_guard lk{mu_};
  current_env_ptr_ = env.ptr;

  // Track unique environments for telemetry
  if (std::find(used_envs_.begin(), used_envs_.end(), env.ptr->region_id) == used_envs_.end()) {
    if (used_envs_.size() < 4) {
      logger_.info("Tracking environment %u for telemetry at x=%.2f", env.ptr->region_id,
                   last_pos_m_);
      used_envs_.push_back(env.ptr->region_id);
      status_.environment_ids[status_.num_environments_used++].id = env.ptr->region_id;
    }
  }

  // Proactive update: If we are mid-route, update the motor sequence for the new environment
  if (route_active_) {
    publish_motor_sequence_for_current_node();
  }
}

void AutonomousService::on_message(const PowerPayload& power) {
  std::lock_guard lk{mu_};
  float watt = power.voltage_v * power.current_a;
  current_total_energy_j_ += watt * 0.01f;
}

void AutonomousService::on_message(const KinematicsPayload& kin) {
  std::lock_guard lk{mu_};
  if (!route_active_ || current_node_idx_ >= cmd_.num_nodes) {
    last_pos_m_ = kin.position_m;
    return;
  }

  const auto& node = cmd_.route[current_node_idx_];

  // Phase 2: Check environment bounds if enabled
  if (cmd_.use_environment_tuning) {
    if (!current_env_ptr_ || kin.position_m < current_env_ptr_->bounds.min_pt.x ||
        kin.position_m > current_env_ptr_->bounds.max_pt.x) {
      // Request environment data via internal bus
      bus_.publish<MsgId::InternalEnvRequest>(InternalEnvRequestPayload{kin.position_m, 0.0f});
    }
  }

  // Phase 1/2: Check if we've reached or passed the target
  bool reached = false;
  float prev_dist_vec = node.target_pos.x - last_pos_m_;
  float curr_dist_vec = node.target_pos.x - kin.position_m;

  if (std::abs(curr_dist_vec) < 0.1f) {
    reached = true;  // Proximity
  } else if (prev_dist_vec * curr_dist_vec <= 0) {
    reached = true;  // Crossing
  }

  if (reached) {
    logger_.info("Reached node %u", current_node_idx_);

    // Phase 3: Record stats
    if (status_.num_stats < 8) {
      auto& stats = status_.node_stats[status_.num_stats++];
      stats.initial_energy_mj = node_start_energy_j_ * 1000.0f;
      stats.final_energy_mj = current_total_energy_j_ * 1000.0f;
      stats.total_energy_used_mj = stats.final_energy_mj - stats.initial_energy_mj;
      float moved = std::abs(kin.position_m - node_start_pos_m_);
      stats.energy_per_meter_mj = (moved > 0.01f) ? (stats.total_energy_used_mj / moved) : 0.0f;
    }

    current_node_idx_++;
    status_.current_node_idx = current_node_idx_;

    if (current_node_idx_ >= cmd_.num_nodes) {
      route_active_ = false;
      status_.route_complete = true;
      logger_.info("Route complete.");

      // Phase 3: Publish final status BEFORE stopping the motor/releasing state
      bus_.publish<MsgId::AutoDriveStatus>(status_);

      // Stop the motor
      MotorSequencePayload seq{};
      seq.num_steps = 1;
      seq.steps[0] = {0, 0};  // Stop
      bus_.publish<MsgId::MotorSequence>(seq);
    } else {
      // Start next node immediately
      publish_motor_sequence_for_current_node();
    }
  }
  last_pos_m_ = kin.position_m;
}

void AutonomousService::on_message(const PhysicsTickPayload&) {
  std::lock_guard lk{mu_};
  if (route_active_) {
    bus_.publish<MsgId::KinematicsRequest>({0});
    bus_.publish<MsgId::PowerRequest>({0});
  }
}

void AutonomousService::publish_motor_sequence_for_current_node() {
  const auto& node = cmd_.route[current_node_idx_];

  float target_speed = 0.0f;
  if (current_env_ptr_) {
    target_speed = current_env_ptr_->max_speed_rpm;
  } else {
    target_speed = 1000.0f;  // Default if no environment data yet
  }

  // Apply DriveMode scaling
  switch (cmd_.mode) {
    case DriveMode::Eco:
      target_speed *= 0.75f;
      break;
    case DriveMode::Performance:
      target_speed *= 1.10f;
      break;
    case DriveMode::ManualTuning:
      target_speed *= cmd_.p_gain;
      break;
  }

  // Simple environment tuning: half speed on steep inclines (>5%)
  if (cmd_.use_environment_tuning && current_env_ptr_) {
    if (std::abs(current_env_ptr_->incline_percent) > 5.0f) {
      target_speed /= 2.0f;
      logger_.info("Steep incline (%.1f%%) detected - reducing speed to %d RPM",
                   current_env_ptr_->incline_percent, static_cast<int>(target_speed));
    }
  }

  int16_t speed = static_cast<int16_t>(target_speed);

  MotorSequencePayload seq{};
  seq.cmd_id = 999;
  seq.num_steps = 1;
  seq.steps[0] = {speed, node.timeout_ms * 1000u};

  bus_.publish<MsgId::MotorSequence>(seq);
}

}  // namespace sil
