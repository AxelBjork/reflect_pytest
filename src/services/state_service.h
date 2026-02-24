#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "component.h"
#include "component_logger.h"
#include "message_bus.h"
#include "messages.h"

namespace sil {

class DOC_DESC(
    "Maintains the central lifecycle state machine and the master simulation clock.\n\n"
    "This component tracks the system state and generates the 100Hz `PhysicsTick` "
    "heartbeat that drives all other simulation services.") StateService {
 public:
  using Subscribes =
      ipc::MsgList<ipc::MsgId::StateChange, ipc::MsgId::StateRequest, ipc::MsgId::MotorStatus>;
  using Publishes = ipc::MsgList<ipc::MsgId::StateData, ipc::MsgId::PhysicsTick>;

  explicit StateService(ipc::MessageBus& bus);
  ~StateService();

  StateService(const StateService&) = delete;
  StateService& operator=(const StateService&) = delete;

  void on_message(const ipc::StateChangePayload& sc);
  void on_message(const ipc::StateRequestPayload&);
  void on_message(const ipc::MotorStatusPayload& ms);

 private:
  ipc::TypedPublisher<StateService> bus_;
  ComponentLogger logger_;
  std::mutex mu_;
  ipc::SystemState state_{ipc::SystemState::Ready};

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
