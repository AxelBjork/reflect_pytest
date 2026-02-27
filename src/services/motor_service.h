#pragma once
#include <mutex>

#include "component.h"
#include "component_logger.h"
#include "core_msgs.h"
#include "simulation_msgs.h"

namespace sil {

class DOC_DESC(
    "Reactive service that executes motor command sequences.\n\n"
    "This service is responsible for stepping through a sequence of motor commands "
    "in response to `PhysicsTick` events, and reporting its status via `MotorStatus` messages.")
    MotorService {
 public:
  using Subscribes = ipc::MsgList<MsgId::MotorSequence, MsgId::PhysicsTick>;
  using Publishes = ipc::MsgList<MsgId::MotorStatus>;

  explicit MotorService(ipc::MessageBus& bus);
  ~MotorService() = default;

  MotorService(const MotorService&) = delete;
  MotorService& operator=(const MotorService&) = delete;

  void on_message(const MotorSequencePayload& cmd);
  void on_message(const PhysicsTickPayload& tick);

 private:
  ipc::TypedPublisher<MotorService> bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  bool active_{false};
  MotorSequencePayload current_cmd_{};
  uint8_t current_step_idx_{0};
  uint32_t step_remaining_us_{0};

  void advance_step();
};

}  // namespace sil
