#pragma once
// simulator.h — Motor sequence simulator component.

#include <condition_variable>
#include <mutex>
#include <thread>

#include "message_bus.h"

namespace sil {

// Physics constants (deliberate round numbers for testability).
inline constexpr float K_RPM_TO_MPS = 0.01f;    // m/s per RPM
inline constexpr float K_RPM_TO_AMPS = 0.005f;  // A per RPM
inline constexpr float R_INT_OHM = 0.5f;        // internal resistance
inline constexpr float V_MAX = 12.6f;           // fully charged
inline constexpr float V_MIN = 10.5f;           // depleted

class Simulator {
 public:
  explicit Simulator(ipc::MessageBus& bus);
  ~Simulator();

  Simulator(const Simulator&) = delete;
  Simulator& operator=(const Simulator&) = delete;

 private:
  ipc::MessageBus& bus_;

  // --- Protected by mu_ ---
  struct SimState {
    uint32_t cmd_id = 0;
    uint32_t elapsed_us = 0;
    float position_m = 0.0f;
    float speed_mps = 0.0f;
    float voltage_v = V_MAX;
    float current_a = 0.0f;
    uint8_t soc = 100;
    ipc::SystemState sys_state = ipc::SystemState::Ready;
    bool have_cmd = false;
    ipc::MotorSequencePayload pending_cmd{};
  } state_;
  std::mutex mu_;
  std::condition_variable cv_;

  std::thread exec_thread_;
  std::thread log_thread_;
  std::atomic<bool> running_{true};

  void exec_loop();
  void log_loop();
  void tick_physics(int16_t speed_rpm, uint32_t dt_us);
  void publish_log_status();
};

}  // namespace sil
