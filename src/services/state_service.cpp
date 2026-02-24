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

void StateService::on_message(const ipc::StateChangePayload& sc) {
  logger_.info("State transition to %u", (uint32_t)sc.state);
  std::lock_guard lk{mu_};
  state_ = sc.state;
}

void StateService::on_message(const ipc::StateRequestPayload&) {
  ipc::StatePayload r{};
  {
    std::unique_lock lk{mu_};
    r.state = state_;
  }
  bus_.publish<ipc::MsgId::StateData>(r);
}

void StateService::on_message(const ipc::MotorStatusPayload& ms) {
  std::lock_guard lk{mu_};
  last_cmd_id_ = ms.cmd_id;
  last_rpm_ = ms.speed_rpm;
  last_active_ = ms.is_active;
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
      active = last_active_ && (state_ == ipc::SystemState::Executing);
    }

    if (active) {
      bus_.publish<ipc::MsgId::PhysicsTick>({cmd_id, rpm, kTickUs});
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
