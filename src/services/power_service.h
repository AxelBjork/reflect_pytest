#pragma once
#include <algorithm>
#include <cmath>
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

  explicit PowerService(ipc::MessageBus& bus) : bus_(bus), logger_("power") {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const PhysicsTickPayload& tick) {
    std::lock_guard lk{mu_};
    float dt_s = tick.dt_us / 1e6f;
    float rpm_abs = std::abs(static_cast<float>(tick.speed_rpm));
    current_a_ = I_IDLE_A + K_RPM_POW_TO_AMPS * std::pow(rpm_abs, RPM_EXP_P);
    float dv = current_a_ * R_INT_OHM * dt_s;
    voltage_v_ = std::max(V_MIN, voltage_v_ - dv);
    soc_ = static_cast<uint8_t>(
        std::clamp((voltage_v_ - V_MIN) / (V_MAX - V_MIN) * 100.0f, 0.0f, 100.0f));
    cmd_id_ = tick.cmd_id;

    // Log every 100 ticks (approx 1s)
    static uint32_t count = 0;
    if (++count % 100 == 0) {
      logger_.info("Voltage: %.2fV, Current: %.3fA, SOC: %u%%", voltage_v_, current_a_, soc_);
    }
  }

  void on_message(const MotorStatusPayload& ms) {
    std::lock_guard lk{mu_};
    cmd_id_ = ms.cmd_id;
    if (!ms.is_active) {
      current_a_ = 0.0f;
    }
  }

  void on_message(const PowerRequestPayload&) {
    PowerPayload p{};
    {
      std::lock_guard lk{mu_};
      p.cmd_id = cmd_id_;
      p.voltage_v = voltage_v_;
      p.current_a = current_a_;
      p.state_of_charge = soc_;
    }
    bus_.publish<MsgId::PowerData>(p);
  }

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
