#pragma once
#include <mutex>

#include "publisher.h"
#include "component_logger.h"
#include "core_msgs.h"
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

  explicit KinematicsService(ipc::MessageBus& bus);

  void on_message(const PhysicsTickPayload& tick);
  void on_message(const MotorStatusPayload& ms);
  void on_message(const KinematicsRequestPayload& req);

 private:
  ipc::TypedPublisher<KinematicsService> bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  uint32_t cmd_id_{0};
  uint32_t elapsed_us_{0};
  float position_m_{0.0f};
  float speed_mps_{0.0f};
  bool active_{false};
};

}  // namespace sil
