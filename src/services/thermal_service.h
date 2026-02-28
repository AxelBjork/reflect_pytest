#pragma once
#include <mutex>

#include "autonomous_msgs.h"
#include "component.h"
#include "component_logger.h"
#include "core_msgs.h"
#include "message_bus.h"
#include "simulation_msgs.h"

namespace sil {

class DOC_DESC(
    "Simulates basic motor and battery temperature dynamics.\n\n"
    "On each physics tick, the service updates motor and battery temperatures using a simple "
    "first-order heat balance: a speed-proportional heat generation term minus a linear cooling "
    "term to ambient. Ambient temperature is updated from EnvironmentData. A ThermalRequest "
    "publishes the latest temperatures.\n\n"
    "Motor model:\n"
    "$$ \\dot{T}_m = q_m - c_m \\,(T_m - T_a) $$\n"
    "$$ q_m = 0.005\\,|\\mathrm{RPM}|,\\quad c_m = 0.05 $$\n\n"
    "Battery model:\n"
    "$$ \\dot{T}_b = q_b - c_b \\,(T_b - T_a) $$\n"
    "$$ q_b = 0.002\\,|\\mathrm{RPM}|,\\quad c_b = 0.02 $$\n\n"
    "Discrete update per tick (for each body):\n"
    "$$ T \\leftarrow T + \\bigl(\\dot{T}\\bigr)\\,\\Delta t $$\n\n"
    "Where $T_a$ is ambient temperature from EnvironmentData, and $\\Delta t$ is the physics "
    "timestep in seconds.") ThermalService {
 public:
  using Subscribes =
      ipc::MsgList<MsgId::PhysicsTick, MsgId::EnvironmentData, MsgId::ThermalRequest>;
  using Publishes = ipc::MsgList<MsgId::ThermalData>;

  explicit ThermalService(ipc::MessageBus& bus);

  void on_message(const PhysicsTickPayload& tick);
  void on_message(const EnvironmentPayload& env);
  void on_message(const ThermalRequestPayload& req);

 private:
  void publish_data();

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  float ambient_temp_c_{20.0f};
  float motor_temp_c_{20.0f};
  float battery_temp_c_{20.0f};
};

}  // namespace sil
