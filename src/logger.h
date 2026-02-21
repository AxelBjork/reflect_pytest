#pragma once
#include <memory>

#include "ipc/message_bus.hpp"

namespace sil {
// Opaque pointer wrapper to prevent dragging `<mutex>` into main
class Logger;
void destroy_logger(Logger* logger);

struct LoggerDeleter {
  void operator()(Logger* p) const {
    destroy_logger(p);
  }
};

using LoggerPtr = std::unique_ptr<Logger, LoggerDeleter>;

LoggerPtr create_logger(ipc::MessageBus& bus);
}  // namespace sil
