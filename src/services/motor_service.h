#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "component.h"
#include "component_logger.h"
#include "messages.h"

namespace sil {

class DOC_DESC(
    "Manages the thread that executes timed motor commands in real-time.\n\n"
    "This service is responsible for stepping through a sequence of motor commands, "
    "emitting standard `PhysicsTick` events at 100Hz, and broadcasting `StateChange` "
    "events when a sequence starts or finishes.") MotorService {
 public:
  using Subscribes = ipc::MsgList<ipc::MsgId::MotorSequence>;
  using Publishes = ipc::MsgList<ipc::MsgId::PhysicsTick, ipc::MsgId::StateChange>;

  explicit MotorService(ipc::MessageBus& bus);
  ~MotorService();

  MotorService(const MotorService&) = delete;
  MotorService& operator=(const MotorService&) = delete;

  void on_message(const ipc::MotorSequencePayload& cmd);

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::thread exec_thread_;
  std::atomic<bool> running_{true};

  bool have_cmd_{false};
  ipc::MotorSequencePayload pending_cmd_{};

  void exec_loop();
};

}  // namespace sil
