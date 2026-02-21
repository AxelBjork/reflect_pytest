#pragma once
#include <mutex>

#include "component.h"

namespace sil {

class StateService {
 public:
  using Subscribes = ipc::MsgList<ipc::MsgId::StateChange, ipc::MsgId::QueryState>;
  using Publishes = ipc::MsgList<ipc::MsgId::QueryState>;

  explicit StateService(ipc::MessageBus& bus) : bus_(bus) {
    ipc::bind_subscriptions(bus_, this);
  }

  void on_message(const ipc::StateChangePayload& sc) {
    std::lock_guard lk{mu_};
    state_ = sc.state;
  }

  void on_message(const ipc::QueryStatePayload&) {
    ipc::QueryStatePayload r{};
    {
      std::lock_guard lk{mu_};
      r.state = state_;
    }
    bus_.publish<ipc::MsgId::QueryState>(r);
  }

 private:
  ipc::MessageBus& bus_;
  std::mutex mu_;
  ipc::SystemState state_{ipc::SystemState::Ready};
};

}  // namespace sil
