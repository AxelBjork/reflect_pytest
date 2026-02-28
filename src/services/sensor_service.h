#pragma once
#include <mutex>

#include "autonomous_msgs.h"
#include "component.h"
#include "component_logger.h"
#include "core_msgs.h"
#include "message_bus.h"

namespace sil {

class DOC_DESC(
    "Sensor service that procedurally generates environment data based on "
    "requested locations.\n\n"
    "Simulates reading from physical terrain maps with a 10us delay.") SensorService {
 public:
  using Subscribes = ipc::MsgList<MsgId::SensorRequest, MsgId::StateData>;
  using Publishes = ipc::MsgList<MsgId::SensorAck, MsgId::EnvironmentData>;

  explicit SensorService(ipc::MessageBus& bus);

  void on_message(const SensorRequestPayload& req);
  void on_message(const StatePayload& state_msg);

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;

  SystemState state_{SystemState::Init};
  uint32_t seed_{42};  // Private seed for deterministic generation
  uint32_t region_counter_{1000};
};

}  // namespace sil
