#include "services/environment_service.h"

#include <algorithm>

namespace sil {

EnvironmentService::EnvironmentService(ipc::MessageBus& bus) : bus_(bus), logger_("env") {
  ipc::bind_subscriptions(bus_, this);
}

void EnvironmentService::on_message(const EnvironmentPayload& env) {
  std::lock_guard lk{mu_};

  // Update or insert into cache
  auto it = std::find_if(cache_.begin(), cache_.end(),
                         [&](const auto& e) { return e->region_id == env.region_id; });
  if (it != cache_.end()) {
    **it = env;
  } else {
    cache_.push_back(std::make_shared<::EnvironmentPayload>(env));
  }

  logger_.info("Environment updated: region %u, Temp %.1fC, Incline %.1f%%, Friction %.2f",
               env.region_id, env.ambient_temp_c, env.incline_percent, env.surface_friction);

  // ACK receipt of the data (EnvironmentData is always external)
  bus_.publish<MsgId::EnvironmentAck>(::EnvironmentAckPayload{env.region_id});
}

void EnvironmentService::on_message(const InternalEnvRequestPayload& req) {
  std::lock_guard lk{mu_};
  for (const auto& env : cache_) {
    if (req.x >= env->bounds.min_pt.x && req.x <= env->bounds.max_pt.x &&
        req.y >= env->bounds.min_pt.y && req.y <= env->bounds.max_pt.y) {
      logger_.info("Internal pointer hit: sharing region %u", env->region_id);
      bus_.publish<MsgId::InternalEnvData>(::InternalEnvDataPayload{env});
      return;
    }
  }

  // Miss: Trigger external request via bus, but throttle it
  auto now = std::chrono::steady_clock::now();
  if (now - last_request_time_ > std::chrono::milliseconds(500)) {
    logger_.info("Internal miss for (%.1f, %.1f) -> fetching from Python", req.x, req.y);
    EnvironmentRequestPayload ext_req;
    ext_req.target_location = {req.x, req.y};
    bus_.publish<MsgId::EnvironmentRequest>(ext_req);
    last_request_time_ = now;
  }
}

}  // namespace sil
