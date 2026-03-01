#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

#include "autonomous_msgs.h"
#include "publisher.h"
#include "component_logger.h"

namespace sil {

class DOC_DESC(
    "Centralized service for managing environmental simulation data.\n\n"
    "It maintains a spatial cache of environmental regions (temperature, incline, friction) "
    "and provides efficient, lifetime-tracked access to this data via in-process bus messages. "
    "By publishing InternalEnvData messages containing std::shared_ptr, it allows consumers "
    "like AutonomousService to access stable map regions without data copying or direct component "
    "coupling.") EnvironmentService {
 public:
  using Subscribes = ipc::MsgList<MsgId::EnvironmentData, MsgId::InternalEnvRequest>;
  using Publishes =
      ipc::MsgList<MsgId::EnvironmentAck, MsgId::EnvironmentRequest, MsgId::InternalEnvData>;

  explicit EnvironmentService(ipc::MessageBus& bus);

  void on_message(const EnvironmentPayload& env);
  void on_message(const InternalEnvRequestPayload& req);

 private:
  ipc::TypedPublisher<EnvironmentService> bus_;
  ComponentLogger logger_;
  std::recursive_mutex mu_;

  std::vector<std::shared_ptr<::EnvironmentPayload>> cache_;
  std::chrono::steady_clock::time_point last_request_time_{};
};

}  // namespace sil
