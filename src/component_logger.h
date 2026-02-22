#pragma once

#include <cstdarg>
#include <cstdio>

#include "message_bus.h"
#include "messages.h"

namespace sil {

class ComponentLogger {
 public:
  ComponentLogger(ipc::MessageBus& bus, ipc::ComponentId component)
      : bus_(bus), component_(component) {
  }

  void debug(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));
  void info(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));
  void warn(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));
  void error(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));

 private:
  ipc::MessageBus& bus_;
  ipc::ComponentId component_;

  void log_valist(ipc::Severity severity, const char* fmt, va_list args) const;
};

}  // namespace sil
