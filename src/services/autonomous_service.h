#pragma once
#include <memory>
#include <mutex>
#include <vector>

#include "autonomous_msgs.h"
#include "publisher.h"
#include "component_logger.h"
#include "core_msgs.h"
#include "simulation_msgs.h"

namespace sil {

class DOC_DESC(
    "High-level autonomous driving service executing a waypoint (node) route.\n\n"
    "The service accepts an AutoDriveCommand containing a list of ManeuverNodes (1D x targets). "
    "While a route is active, it periodically requests Kinematics and Power data on each physics "
    "tick, decides when the current node has been reached, and publishes MotorSequence commands "
    "to drive toward the node.") AutonomousService {
 public:
  using Subscribes = ipc::MsgList<MsgId::AutoDriveCommand, MsgId::KinematicsData,
                                  MsgId::PhysicsTick, MsgId::PowerData, MsgId::InternalEnvData>;
  using Publishes =
      ipc::MsgList<MsgId::MotorSequence, MsgId::AutoDriveStatus, MsgId::KinematicsRequest,
                   MsgId::PowerRequest, MsgId::InternalEnvRequest>;

  explicit AutonomousService(ipc::MessageBus& bus);

  void on_message(const AutoDriveCommandPayload& cmd);
  void on_message(const InternalEnvDataPayload& env);
  void on_message(const PowerPayload& power);
  void on_message(const KinematicsPayload& kin);
  void on_message(const PhysicsTickPayload& tick);

 private:
  void publish_motor_sequence_for_current_node();

  ipc::TypedPublisher<AutonomousService> bus_;
  ComponentLogger logger_;
  std::recursive_mutex mu_;

  AutoDriveCommandPayload cmd_{};
  uint8_t current_node_idx_{0};
  bool route_active_{false};

  // Lifetime-tracked pointer to active environment
  std::shared_ptr<const EnvironmentPayload> current_env_ptr_{nullptr};

  // Phase 3: Telemetry
  AutoDriveStatusPayload status_{};
  float current_total_energy_j_{0.0f};
  float node_start_energy_j_{0.0f};
  float node_start_pos_m_{0.0f};
  float last_pos_m_{0.0f};
  std::vector<uint32_t> used_envs_;
};

}  // namespace sil
