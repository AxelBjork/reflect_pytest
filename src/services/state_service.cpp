#include "services/state_service.h"

#include <chrono>

namespace sil {

static constexpr uint32_t kTickUs = 10'000;  // 100Hz

StateService::StateService(ipc::MessageBus& bus) : bus_(bus), logger_("state") {
  ipc::bind_subscriptions(bus, this);
  clock_thread_ = std::thread([this] { clock_loop(); });
}

StateService::~StateService() {
  running_ = false;
  cv_.notify_all();
  if (clock_thread_.joinable()) clock_thread_.join();
}

void StateService::on_message(const StateRequestPayload&) {
  StatePayload r{};
  {
    std::unique_lock lk{mu_};
    r.state = state_;
  }
  bus_.publish<MsgId::StateData>(r);
}

void StateService::on_message(const MotorStatusPayload& ms) {
  std::lock_guard lk{mu_};
  last_cmd_id_ = ms.cmd_id;
  last_rpm_ = ms.speed_rpm;
  last_active_ = ms.is_active;

  SystemState next_state = ms.is_active ? SystemState::Executing : SystemState::Ready;
  if (next_state != state_) {
    state_ = next_state;
    logger_.info("State transition to %u", (uint32_t)state_);
  }
}

void StateService::clock_loop() {
  while (running_) {
    auto start = std::chrono::steady_clock::now();

    uint32_t cmd_id = 0;
    int16_t rpm = 0;
    bool active = false;

    {
      std::lock_guard lk{mu_};
      cmd_id = last_cmd_id_;
      rpm = last_rpm_;
      // We only report RPM if we are actually in Executing state.
      // Otherwise report 0 to kinematics/power listeners.
      if (state_ != SystemState::Executing) {
        rpm = 0;
      }

      bus_.publish<MsgId::PhysicsTick>({cmd_id, rpm, kTickUs});
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    if (elapsed.count() < kTickUs) {
      std::unique_lock lk{mu_};
      cv_.wait_for(lk, std::chrono::microseconds(kTickUs - elapsed.count()),
                   [this] { return !running_.load(); });
    }
  }
}

}  // namespace sil
