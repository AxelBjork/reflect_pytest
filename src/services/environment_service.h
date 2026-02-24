#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

#include "component.h"
#include "component_logger.h"
#include "internal_messages.h"
#include "message_bus.h"
#include "messages.h"

namespace sil {

class DOC_DESC(
    "Centralized service for managing environmental simulation data.\n\n"
    "It maintains a spatial cache of environmental regions (temperature, incline, friction) "
    "and provides efficient, lifetime-tracked access to this data via in-process bus messages. "
    "By publishing InternalEnvData messages containing std::shared_ptr, it allows consumers "
    "like AutonomousService to access stable map regions without data copying or direct component "
    "coupling.") EnvironmentService {
 public:
  using Subscribes = ipc::MsgList<ipc::MsgId::EnvironmentData, ipc::MsgId::ResetRequest,
                                  ipc::MsgId::InternalEnvRequest, ipc::MsgId::EnvironmentRequest>;
  using Publishes = ipc::MsgList<ipc::MsgId::EnvironmentAck, ipc::MsgId::EnvironmentRequest,
                                 ipc::MsgId::InternalEnvData>;

  explicit EnvironmentService(ipc::MessageBus& bus);

  void on_message(const ipc::EnvironmentPayload& env);
  void on_message(const ipc::EnvironmentRequestPayload& req);
  void on_message(const ipc::ResetRequestPayload& req);
  void on_message(const ipc::InternalEnvRequestPayload& req);

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::recursive_mutex mu_;

  std::vector<std::shared_ptr<ipc::EnvironmentPayload>> cache_;
  std::chrono::steady_clock::time_point last_request_time_{};
};

}  // namespace sil
