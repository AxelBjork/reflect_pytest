#include "component_logger.h"

#include "services/log_service.h"

namespace sil {

LogService* ComponentLogger::sink_ = nullptr;

void ComponentLogger::init(LogService& service) {
  sink_ = &service;
}

void ComponentLogger::log_valist(Severity severity, const char* fmt, va_list args) const {
  if (!sink_) return;

  LogPayload p{};
  p.severity = severity;
  std::strncpy(p.component, name_, sizeof(p.component) - 1);
  std::vsnprintf(p.text, sizeof(p.text), fmt, args);
  sink_->log(p);
}

void ComponentLogger::debug(const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  log_valist(Severity::Debug, fmt, args);
  va_end(args);
}

void ComponentLogger::info(const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  log_valist(Severity::Info, fmt, args);
  va_end(args);
}

void ComponentLogger::warn(const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  log_valist(Severity::Warn, fmt, args);
  va_end(args);
}

void ComponentLogger::error(const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  log_valist(Severity::Error, fmt, args);
  va_end(args);
}

}  // namespace sil
