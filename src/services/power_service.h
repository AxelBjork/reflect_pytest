#pragma once
#include <algorithm>
#include <cmath>
#include <mutex>

#include "component.h"

namespace sil {

inline constexpr float K_RPM_TO_AMPS = 0.005f;
inline constexpr float R_INT_OHM = 0.5f;
inline constexpr float V_MAX = 12.6f;
inline constexpr float V_MIN = 10.5f;

class DOC_DESC(
    "Models a simple battery pack. Voltage drops dynamically under motor load based on "
    "internal resistance, tracking state of charge over time.") PowerService {
 public:
  using Subscribes =
      ipc::MsgList<ipc::MsgId::PhysicsTick, ipc::MsgId::PowerRequest, ipc::MsgId::StateChange>;
  using Publishes = ipc::MsgList<ipc::MsgId::PowerData>;

  explicit PowerService(ipc::MessageBus& bus) : bus_(bus) {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const ipc::PhysicsTickPayload& tick) {
    std::lock_guard lk{mu_};
    float dt_s = tick.dt_us / 1e6f;
    current_a_ = std::abs(tick.speed_rpm) * K_RPM_TO_AMPS;
    float dv = current_a_ * R_INT_OHM * dt_s;
    voltage_v_ = std::max(V_MIN, voltage_v_ - dv);
    soc_ = static_cast<uint8_t>(
        std::clamp((voltage_v_ - V_MIN) / (V_MAX - V_MIN) * 100.0f, 0.0f, 100.0f));
  }

  void on_message(const ipc::StateChangePayload& sc) {
    std::lock_guard lk{mu_};
    cmd_id_ = sc.cmd_id;
    if (sc.state == ipc::SystemState::Ready) {
      current_a_ = 0.0f;
    }
  }

  void on_message(const ipc::PowerRequestPayload&) {
    ipc::PowerPayload p{};
    {
      std::lock_guard lk{mu_};
      p.cmd_id = cmd_id_;
      p.voltage_v = voltage_v_;
      p.current_a = current_a_;
      p.state_of_charge = soc_;
    }
    bus_.publish<ipc::MsgId::PowerData>(p);
  }

 private:
  ipc::MessageBus& bus_;
  std::mutex mu_;

  uint32_t cmd_id_{0};
  float voltage_v_{V_MAX};
  float current_a_{0.0f};
  uint8_t soc_{100};
};

}  // namespace sil
