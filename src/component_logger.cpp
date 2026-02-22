#include "component_logger.h"

namespace sil {

void ComponentLogger::log_valist(ipc::Severity severity, const char* fmt, va_list args) const {
  ipc::LogPayload p{};
  p.severity = severity;
  p.component = component_;
  std::vsnprintf(p.text, sizeof(p.text), fmt, args);
  bus_.publish<ipc::MsgId::Log>(p);
}

void ComponentLogger::debug(const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  log_valist(ipc::Severity::Debug, fmt, args);
  va_end(args);
}

void ComponentLogger::info(const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  log_valist(ipc::Severity::Info, fmt, args);
  va_end(args);
}

void ComponentLogger::warn(const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  log_valist(ipc::Severity::Warn, fmt, args);
  va_end(args);
}

void ComponentLogger::error(const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  log_valist(ipc::Severity::Error, fmt, args);
  va_end(args);
}

}  // namespace sil
