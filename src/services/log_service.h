#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

#include "component.h"

namespace sil {

class DOC_DESC(
    "Periodically aggregates system state into human-readable text logs for debugging.\n\n"
    "It acts as an internal observer, keeping track of the latest kinematics, power, and state "
    "metrics, "
    "and broadcasts a formatted string representation at a fixed interval to track the "
    "simulation's progress.") LogService {
 public:
  using Subscribes = ipc::MsgList<ipc::MsgId::PhysicsTick, ipc::MsgId::StateChange>;
  using Publishes = ipc::MsgList<ipc::MsgId::Log>;

  explicit LogService(ipc::MessageBus& bus);
  ~LogService();

  LogService(const LogService&) = delete;
  LogService& operator=(const LogService&) = delete;

  void on_message(const ipc::PhysicsTickPayload& tick);
  void on_message(const ipc::StateChangePayload& sc);

 private:
  ipc::MessageBus& bus_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::thread log_thread_;
  bool running_{true};

  ipc::SystemState state_{ipc::SystemState::Ready};
  uint32_t cmd_id_{0};
  float speed_mps_{0.0f};
  float pos_m_{0.0f};
  float current_a_{0.0f};
  float voltage_v_{12.6f};

  void log_loop();
};

}  // namespace sil
