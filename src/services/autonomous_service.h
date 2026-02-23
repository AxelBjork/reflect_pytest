#pragma once
#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

#include "component.h"
#include "component_logger.h"
#include "message_bus.h"
#include "messages.h"

namespace sil {

class DOC_DESC("High level autonomous driving service.") AutonomousService {
 public:
  using Subscribes =
      ipc::MsgList<ipc::MsgId::AutoDriveCommand, ipc::MsgId::KinematicsData,
                   ipc::MsgId::PhysicsTick, ipc::MsgId::EnvironmentData, ipc::MsgId::PowerData>;
  using Publishes = ipc::MsgList<ipc::MsgId::MotorSequence, ipc::MsgId::AutoDriveStatus,
                                 ipc::MsgId::KinematicsRequest, ipc::MsgId::PowerRequest,
                                 ipc::MsgId::EnvironmentRequest>;

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

    if (has_env_) {
      used_envs_.push_back(current_env_.region_id);
      status_.environment_ids[status_.num_environments_used++].id = current_env_.region_id;
    }

    current_total_energy_j_ = 0.0f;
    node_start_energy_j_ = 0.0f;
    node_start_pos_m_ = last_pos_m_;

    if (route_active_) {
      publish_motor_sequence_for_current_node();
    }
  }

  void on_message(const ipc::EnvironmentPayload& env) {
    std::lock_guard lk{mu_};
    current_env_ = env;
    has_env_ = true;
    logger_.info("Received environment for region %u", env.region_id);

    // Track unique environments
    if (std::find(used_envs_.begin(), used_envs_.end(), env.region_id) == used_envs_.end()) {
      if (used_envs_.size() < 4) {  // MaxEnvs is 4 in messages.h
        used_envs_.push_back(env.region_id);
        status_.environment_ids[status_.num_environments_used++].id = env.region_id;
      }
    }
  }

  void on_message(const ipc::PowerPayload& power) {
    std::lock_guard lk{mu_};
    // Very simplified energy integration: Power (W) * 0.01 (s) for each PhysicsTick
    // Since PowerData is 100Hz
    float watt = power.voltage_v * power.current_a;
    current_total_energy_j_ += watt * 0.01f;
  }

  void on_message(const ipc::KinematicsPayload& kin) {
    std::lock_guard lk{mu_};
    last_pos_m_ = kin.position_m;
    if (!route_active_ || current_node_idx_ >= cmd_.num_nodes) return;

    const auto& node = cmd_.route[current_node_idx_];
    float dist = std::abs(node.target_pos.x - kin.position_m);

    // Phase 2: Check environment bounds if enabled
    if (cmd_.use_environment_tuning) {
      bool out_of_bounds = !has_env_ || (kin.position_m < current_env_.bounds.min_pt.x) ||
                           (kin.position_m > current_env_.bounds.max_pt.x);
      if (out_of_bounds) {
        request_environment_at(kin.position_m);
      }
    }

    // If we've reached the target
    if (dist < 0.1f) {
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

        // Stop the motor
        ipc::MotorSequencePayload seq{};
        seq.num_steps = 1;
        seq.steps[0] = {0, 0};  // Stop
        bus_.publish<ipc::MsgId::MotorSequence>(seq);
      } else {
        node_start_energy_j_ = current_total_energy_j_;
        node_start_pos_m_ = kin.position_m;
        publish_motor_sequence_for_current_node();
      }

      // Periodically publish status
      bus_.publish<ipc::MsgId::AutoDriveStatus>(status_);
    }
  }

  void on_message(const ipc::PhysicsTickPayload& tick) {
    std::lock_guard lk{mu_};
    if (route_active_) {
      bus_.publish<ipc::MsgId::KinematicsRequest>(ipc::KinematicsRequestPayload{0});
      bus_.publish<ipc::MsgId::PowerRequest>(ipc::PowerRequestPayload{0});
    }
  }

 private:
  void request_environment_at(float pos_x) {
    ipc::EnvironmentRequestPayload req;
    req.target_location = {pos_x, 0.0f};
    bus_.publish<ipc::MsgId::EnvironmentRequest>(req);
  }

  void publish_motor_sequence_for_current_node() {
    const auto& node = cmd_.route[current_node_idx_];

    ipc::MotorSequencePayload seq{};
    seq.cmd_id = 999;
    seq.num_steps = 1;
    seq.steps[0] = {node.speed_limit_rpm, node.timeout_ms * 1000u};

    bus_.publish<ipc::MsgId::MotorSequence>(seq);
  }

  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  ipc::AutoDriveCommandPayload cmd_{};
  uint8_t current_node_idx_{0};
  bool route_active_{false};

  // Phase 2: Environment Tracking
  bool has_env_{false};
  ipc::EnvironmentPayload current_env_{};

  // Phase 3: Telemetry
  ipc::AutoDriveStatusPayload status_{};
  float current_total_energy_j_{0.0f};
  float node_start_energy_j_{0.0f};
  float node_start_pos_m_{0.0f};
  float last_pos_m_{0.0f};
  std::vector<uint32_t> used_envs_;
};

}  // namespace sil
