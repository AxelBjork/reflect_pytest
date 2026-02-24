#pragma once
#include <mutex>

#include "component.h"
#include "component_logger.h"
#include "messages.h"

namespace sil {

class DOC_DESC(
    "Reactive service that executes motor command sequences.\n\n"
    "This service is responsible for stepping through a sequence of motor commands "
    "in response to `PhysicsTick` events, and reporting its status via `MotorStatus` messages.")
    MotorService {
 public:
  using Subscribes =
      ipc::MsgList<ipc::MsgId::MotorSequence, ipc::MsgId::PhysicsTick, ipc::MsgId::ResetRequest>;
  using Publishes = ipc::MsgList<ipc::MsgId::MotorStatus>;

  explicit MotorService(ipc::MessageBus& bus);
  ~MotorService() = default;

  MotorService(const MotorService&) = delete;
  MotorService& operator=(const MotorService&) = delete;

  void on_message(const ipc::MotorSequencePayload& cmd);
  void on_message(const ipc::PhysicsTickPayload& tick);
  void on_message(const ipc::ResetRequestPayload& msg);

 private:
  ipc::TypedPublisher<MotorService> bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  bool active_{false};
  ipc::MotorSequencePayload current_cmd_{};
  uint8_t current_step_idx_{0};
  uint32_t step_remaining_us_{0};

  void advance_step();
};

}  // namespace sil
