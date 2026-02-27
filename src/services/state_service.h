#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "component.h"
#include "component_logger.h"
#include "core_msgs.h"
#include "message_bus.h"
#include "msg_base.h"
#include "simulation_msgs.h"

namespace sil {

class DOC_DESC(
    "Maintains the central lifecycle state machine and the master simulation clock.\n\n"
    "This component tracks the system state and generates the 100Hz `PhysicsTick` "
    "heartbeat that drives all other simulation services.") StateService {
 public:
  using Subscribes = ipc::MsgList<MsgId::StateRequest, MsgId::MotorStatus>;
  using Publishes = ipc::MsgList<MsgId::StateData, MsgId::PhysicsTick>;

  explicit StateService(ipc::MessageBus& bus);
  ~StateService();

  StateService(const StateService&) = delete;
  StateService& operator=(const StateService&) = delete;

  void on_message(const StateRequestPayload&);
  void on_message(const MotorStatusPayload& ms);

 private:
  ipc::TypedPublisher<StateService> bus_;
  ComponentLogger logger_;
  std::mutex mu_;
  SystemState state_{SystemState::Ready};

  // Clock variables
  std::thread clock_thread_;
  std::atomic<bool> running_{true};
  std::condition_variable cv_;

  // Cached motor state for ticking
  uint32_t last_cmd_id_{0};
  int16_t last_rpm_{0};
  bool last_active_{false};

  void clock_loop();
};

}  // namespace sil
