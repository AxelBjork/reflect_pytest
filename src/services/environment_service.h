#pragma once
#include <mutex>

#include "component.h"
#include "component_logger.h"
#include "message_bus.h"
#include "messages.h"

namespace sil {

class DOC_DESC("Environment Service") EnvironmentService {
 public:
  using Subscribes = ipc::MsgList<ipc::MsgId::EnvironmentCommand, ipc::MsgId::EnvironmentRequest>;
  using Publishes = ipc::MsgList<ipc::MsgId::EnvironmentData>;

  explicit EnvironmentService(ipc::MessageBus& bus) : bus_(bus), logger_("env") {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const ipc::EnvironmentCommandPayload& cmd) {
    std::lock_guard lk{mu_};
    env_.ambient_temp_c = cmd.ambient_temp_c;
    env_.incline_percent = cmd.incline_percent;
    env_.surface_friction = cmd.surface_friction;

    // Broadcast newly applied environment conditions
    bus_.publish<ipc::MsgId::EnvironmentData>(env_);
    logger_.info("Environment updated: Temp %.1fC, Incline %.1f%%, Friction %.2f",
                 env_.ambient_temp_c, env_.incline_percent, env_.surface_friction);
  }

  void on_message(const ipc::EnvironmentRequestPayload&) {
    ipc::EnvironmentPayload p;
    {
      std::lock_guard lk{mu_};
      p = env_;
    }
    bus_.publish<ipc::MsgId::EnvironmentData>(p);
  }

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  ipc::EnvironmentPayload env_{20.0f, 0.0f, 1.0f};  // Defaults
};

}  // namespace sil
