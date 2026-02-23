#pragma once
#include <mutex>

#include "component.h"
#include "component_logger.h"
#include "message_bus.h"
#include "messages.h"

namespace sil {

class DOC_DESC("Environment Service") EnvironmentService {
 public:
  using Subscribes = ipc::MsgList<ipc::MsgId::EnvironmentData, ipc::MsgId::EnvironmentRequest>;
  using Publishes = ipc::MsgList<ipc::MsgId::EnvironmentAck>;

  explicit EnvironmentService(ipc::MessageBus& bus) : bus_(bus), logger_("env") {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const ipc::EnvironmentPayload& env) {
    std::lock_guard lk{mu_};
    env_ = env;

    logger_.info(
        "Environment updated (inbound): region %u, Temp %.1fC, Incline %.1f%%, Friction %.2f",
        env.region_id, env.ambient_temp_c, env.incline_percent, env.surface_friction);

    // ACK receipt of the data
    bus_.publish<ipc::MsgId::EnvironmentAck>(ipc::EnvironmentAckPayload{env.region_id});
  }

  void on_message(const ipc::EnvironmentRequestPayload& req) {
    logger_.info("Environment request for (%.1f, %.1f) - expectation is external responder.",
                 req.target_location.x, req.target_location.y);
  }

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  ipc::EnvironmentPayload env_{0, {{0, 0}, {0, 0}}, 20.0f, 0.0f, 1.0f};  // Defaults
};

}  // namespace sil
