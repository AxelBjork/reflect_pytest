#pragma once

#include <cstdarg>
#include <cstdio>

#include "core_msgs.h"
#include "msg_base.h"

namespace sil {

class LogService;

class ComponentLogger {
 public:
  // Global initialization of the logging sink
  static void init(LogService& service);

  explicit ComponentLogger(const char* name) : name_(name) {
  }

  void debug(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));
  void info(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));
  void warn(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));
  void error(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));

 private:
  const char* name_;
  static LogService* sink_;

  void log_valist(Severity severity, const char* fmt, va_list args) const;
};

}  // namespace sil
