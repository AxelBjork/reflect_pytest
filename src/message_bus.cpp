// ipc/message_bus.cpp

#include "message_bus.h"

namespace ipc {

void MessageBus::dispatch(MsgId id, std::any payload) {
  const auto key = static_cast<uint16_t>(id);
  auto it = handlers_.find(key);
  if (it == handlers_.end()) return;
  for (auto& h : it->second) h(payload);
}

}  // namespace ipc
