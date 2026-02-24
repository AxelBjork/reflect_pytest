#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <vector>

#include "component.h"
#include "component_logger.h"
#include "internal_messages.h"
#include "message_bus.h"
#include "messages.h"

namespace sil {

class DOC_DESC("High level autonomous driving service.") AutonomousService {
 public:
  using Subscribes = ipc::MsgList<ipc::MsgId::AutoDriveCommand, ipc::MsgId::KinematicsData,
                                  ipc::MsgId::PhysicsTick, ipc::MsgId::PowerData,
                                  ipc::MsgId::InternalEnvData, ipc::MsgId::ResetRequest>;
  using Publishes = ipc::MsgList<ipc::MsgId::MotorSequence, ipc::MsgId::AutoDriveStatus,
                                 ipc::MsgId::KinematicsRequest, ipc::MsgId::PowerRequest,
                                 ipc::MsgId::InternalEnvRequest>;

  explicit AutonomousService(ipc::MessageBus& bus) : bus_(bus), logger_("auto") {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const ipc::AutoDriveCommandPayload& cmd) {
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

  void on_message(const ipc::ResetRequestPayload& msg) {
    std::lock_guard lk{mu_};
    route_active_ = false;
    current_node_idx_ = 0;
    current_env_ptr_ = nullptr;
    used_envs_.clear();
    status_ = {};
    last_pos_m_ = 0.0f;
    logger_.info("Autonomous state reset");
  }

  void on_message(const ipc::InternalEnvDataPayload& env) {
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
  }

  void on_message(const ipc::PowerPayload& power) {
    std::lock_guard lk{mu_};
    float watt = power.voltage_v * power.current_a;
    current_total_energy_j_ += watt * 0.01f;
  }

  void on_message(const ipc::KinematicsPayload& kin) {
    std::lock_guard lk{mu_};
    if (!route_active_ || current_node_idx_ >= cmd_.num_nodes) {
      last_pos_m_ = kin.position_m;
      return;
    }

    static uint32_t kin_count = 0;
    logger_.info("Pos: %.3f, Node: %u, Env: %s", kin.position_m, current_node_idx_,
                 current_env_ptr_ ? std::to_string(current_env_ptr_->region_id).c_str() : "None");

    const auto& node = cmd_.route[current_node_idx_];
    float dist = std::abs(node.target_pos.x - kin.position_m);

    // Phase 2: Check environment bounds if enabled
    if (cmd_.use_environment_tuning) {
      if (!current_env_ptr_ || kin.position_m < current_env_ptr_->bounds.min_pt.x ||
          kin.position_m > current_env_ptr_->bounds.max_pt.x) {
        // Request environment data via internal bus
        bus_.publish<ipc::MsgId::InternalEnvRequest>(
            ipc::InternalEnvRequestPayload{kin.position_m, 0.0f});

        // If we still don't have it (async), we might need an external request
        // but the EnvironmentService will handle that and publish back once it arrives.
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
        bus_.publish<ipc::MsgId::AutoDriveStatus>(status_);

        // Stop the motor
        ipc::MotorSequencePayload seq{};
        seq.num_steps = 1;
        seq.steps[0] = {0, 0};  // Stop
        bus_.publish<ipc::MsgId::MotorSequence>(seq);
      }
    }
    last_pos_m_ = kin.position_m;
  }

  void on_message(const ipc::PhysicsTickPayload&) {
    std::lock_guard lk{mu_};
    if (route_active_) {
      bus_.publish<ipc::MsgId::KinematicsRequest>({0});
      bus_.publish<ipc::MsgId::PowerRequest>({0});
    }
  }

 private:
  void publish_motor_sequence_for_current_node() {
    const auto& node = cmd_.route[current_node_idx_];

    int16_t speed = node.speed_limit_rpm;
    // Simple environment tuning: half speed on steep inclines (>5%)
    if (cmd_.use_environment_tuning && current_env_ptr_) {
      if (std::abs(current_env_ptr_->incline_percent) > 5.0f) {
        speed /= 2;
        logger_.info("Steep incline (%.1f%%) detected - reducing speed to %d RPM",
                     current_env_ptr_->incline_percent, speed);
      }
    }

    ipc::MotorSequencePayload seq{};
    seq.cmd_id = 999;
    seq.num_steps = 1;
    seq.steps[0] = {speed, node.timeout_ms * 1000u};

    bus_.publish<ipc::MsgId::MotorSequence>(seq);
  }

  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::recursive_mutex mu_;

  ipc::AutoDriveCommandPayload cmd_{};
  uint8_t current_node_idx_{0};
  bool route_active_{false};

  // Lifetime-tracked pointer to active environment
  std::shared_ptr<const ipc::EnvironmentPayload> current_env_ptr_{nullptr};

  // Phase 3: Telemetry
  ipc::AutoDriveStatusPayload status_{};
  float current_total_energy_j_{0.0f};
  float node_start_energy_j_{0.0f};
  float node_start_pos_m_{0.0f};
  float last_pos_m_{0.0f};
  std::vector<uint32_t> used_envs_;
};

}  // namespace sil
