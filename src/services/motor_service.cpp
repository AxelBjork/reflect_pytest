#include "services/motor_service.h"

#include <algorithm>
#include <chrono>

namespace sil {

static constexpr uint32_t kTickUs = 10'000;  // 100Hz

MotorService::MotorService(ipc::MessageBus& bus)
    : bus_(bus), logger_(bus, ipc::ComponentId::Motor) {
  ipc::bind_subscriptions(bus_, this);
  exec_thread_ = std::thread([this] { exec_loop(); });
}

MotorService::~MotorService() {
  {
    std::lock_guard lk{mu_};
    running_ = false;
    have_cmd_ = true;
  }
  cv_.notify_all();
  if (exec_thread_.joinable()) exec_thread_.join();
}

void MotorService::on_message(const ipc::MotorSequencePayload& cmd) {
  logger_.info("Received MotorSequence with %u steps", cmd.num_steps);
  {
    std::lock_guard lk{mu_};
    pending_cmd_ = cmd;
    pending_cmd_.num_steps = std::min(pending_cmd_.num_steps, ipc::kMaxSubCmds);
    have_cmd_ = true;
  }
  cv_.notify_one();
}

void MotorService::exec_loop() {
  while (running_) {
    ipc::MotorSequencePayload cmd{};
    {
      std::unique_lock lk{mu_};
      cv_.wait(lk, [this] { return have_cmd_ || !running_; });
      if (!running_) break;
      cmd = pending_cmd_;
      have_cmd_ = false;
    }

    bus_.publish<ipc::MsgId::StateChange>({ipc::SystemState::Executing, cmd.cmd_id});

    bool preempted = false;

    for (uint8_t i = 0; i < cmd.num_steps && running_; ++i) {
      const auto& step = cmd.steps[i];
      if (step.duration_us == 0) break;

      uint32_t remaining = step.duration_us;
      while (remaining > 0 && running_) {
        uint32_t dt = std::min(remaining, kTickUs);
        std::this_thread::sleep_for(std::chrono::microseconds(dt));

        std::lock_guard sg{mu_};
        if (have_cmd_) {
          preempted = true;
          break;
        }

        bus_.publish<ipc::MsgId::PhysicsTick>({cmd.cmd_id, step.speed_rpm, dt});
        remaining -= dt;
      }
      if (preempted) break;
    }

    if (!preempted && running_) {
      bus_.publish<ipc::MsgId::StateChange>({ipc::SystemState::Ready, cmd.cmd_id});
    }
  }
}

}  // namespace sil
