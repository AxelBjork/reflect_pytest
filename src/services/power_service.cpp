#include "services/power_service.h"

#include <algorithm>
#include <cmath>

namespace sil {

PowerService::PowerService(ipc::MessageBus& bus) : bus_(bus), logger_("power") {
}

void PowerService::on_message(const PhysicsTickPayload& tick) {
  std::lock_guard lk{mu_};
  float dt_s = tick.dt_us / 1e6f;

  float rpm_abs = std::abs(static_cast<float>(tick.speed_rpm));
  float tick_current = I_IDLE_A + K_RPM_POW_TO_AMPS * std::pow(rpm_abs, RPM_EXP_P);

  float dv = tick_current * R_INT_OHM * dt_s;
  voltage_v_ = std::max(V_MIN, voltage_v_ - dv);
  soc_ = static_cast<uint8_t>(
      std::clamp((voltage_v_ - V_MIN) / (V_MAX - V_MIN) * 100.0f, 0.0f, 100.0f));
  cmd_id_ = tick.cmd_id;

  if (!active_) {
    current_a_ = I_IDLE_A;
  } else {
    current_a_ = tick_current;
  }

  // Log every 100 ticks (approx 1s)
  static uint32_t count = 0;
  if (++count % 100 == 0) {
    logger_.info("Voltage: %.2fV, Current: %.3fA, SOC: %u%%", voltage_v_, current_a_, soc_);
  }
}

void PowerService::on_message(const MotorStatusPayload& ms) {
  std::lock_guard lk{mu_};
  cmd_id_ = ms.cmd_id;
  active_ = ms.is_active;
  if (!ms.is_active) {
    current_a_ = I_IDLE_A;
  }
}

void PowerService::on_message(const PowerRequestPayload&) {
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

}  // namespace sil
