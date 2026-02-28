#pragma once
#include <mutex>

#include "component.h"
#include "component_logger.h"
#include "core_msgs.h"
#include "message_bus.h"
#include "simulation_msgs.h"

namespace sil {

inline constexpr float I_IDLE_A = 0.1f;
inline constexpr float K_RPM_POW_TO_AMPS = 1.863e-4f;  // A / RPM^p (tuned for 200 RPM -> 1.0A)
inline constexpr float RPM_EXP_P = 1.6f;               // p (tune)
inline constexpr float R_INT_OHM = 0.5f;
inline constexpr float V_MAX = 12.6f;
inline constexpr float V_MIN = 10.5f;

class DOC_DESC(
    "Models a simple battery pack dynamically responding to motor load.\n\n"
    "The simulation estimates motor current draw from speed using a non-linear power-law curve, "
    "then applies Ohm's law over the internal resistance to calculate the per-tick voltage drop. "
    "The state of charge (SOC) is linearly interpolated between the maximum and minimum voltage "
    "limits.\n\n"
    "$$ I = I_{idle} + k\\,|\\mathrm{RPM}|^{p} \\quad (\\mathrm{A}) $$\n\n"
    "$$ V \\mathrel{-}= I\\,R_{int}\\,\\Delta t $$\n\n"
    "$$ SOC = \\frac{V - V_{min}}{V_{max} - V_{min}} \\times 100 $$") PowerService {
 public:
  using Subscribes = ipc::MsgList<MsgId::PhysicsTick, MsgId::PowerRequest, MsgId::MotorStatus>;
  using Publishes = ipc::MsgList<MsgId::PowerData>;

  explicit PowerService(ipc::MessageBus& bus);

  void on_message(const PhysicsTickPayload& tick);
  void on_message(const MotorStatusPayload& ms);
  void on_message(const PowerRequestPayload& req);

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  uint32_t cmd_id_{0};
  float voltage_v_{V_MAX};
  float current_a_{0.0f};
  uint8_t soc_{100};
};

}  // namespace sil
