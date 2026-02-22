#pragma once
#include <mutex>

#include "component.h"
#include "component_logger.h"
#include "message_bus.h"
#include "messages.h"

namespace sil {

class DOC_DESC(
    "Maintains the central lifecycle state machine of the simulation.\n\n"
    "This component passively tracks the top-level simulated system state (Init, Ready, Executing, "
    "Fault) "
    "by listening to internal state transitions and makes it available to external clients via "
    "ping requests.") StateService {
 public:
  using Subscribes = ipc::MsgList<ipc::MsgId::StateChange, ipc::MsgId::StateRequest>;
  using Publishes = ipc::MsgList<ipc::MsgId::StateData>;

  explicit StateService(ipc::MessageBus& bus) : bus_(bus), logger_(bus, ipc::ComponentId::State) {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const ipc::StateChangePayload& sc) {
    logger_.info("State transition to %u", (uint32_t)sc.state);
    std::lock_guard lk{mu_};
    state_ = sc.state;
  }

  void on_message(const ipc::StateRequestPayload&) {
    ipc::StatePayload r{};
    {
      std::lock_guard lk{mu_};
      r.state = state_;
    }
    bus_.publish<ipc::MsgId::StateData>(r);
  }

 private:
  ipc::MessageBus& bus_;
  ComponentLogger logger_;
  std::mutex mu_;
  ipc::SystemState state_{ipc::SystemState::Ready};
};

}  // namespace sil
