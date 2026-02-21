#pragma once

#include <tuple>

#include "services/kinematics_service.h"
#include "services/log_service.h"
#include "services/motor_service.h"
#include "services/power_service.h"
#include "services/state_service.h"
#include "udp_bridge.h"

namespace sil {

// A compile-time list of all services instantiated by the main application.
// This is used by the documentation generator to automatically reflect over
// the aggregate Subscribes/Publishes traits of the entire application.
using AppServices = std::tuple<MotorService, KinematicsService, PowerService, StateService,
                               LogService, ipc::UdpBridge>;

template <typename... Ts>
struct AppServicesContainer {
  std::tuple<Ts...> services;

  AppServicesContainer(ipc::MessageBus& bus) : services(((void)sizeof(Ts), bus)...) {
  }
};

template <typename... Ts>
AppServicesContainer<Ts...> create_services_impl(ipc::MessageBus& bus,
                                                 std::type_identity<std::tuple<Ts...>>) {
  return AppServicesContainer<Ts...>(bus);
}

inline auto create_app_services(ipc::MessageBus& bus) {
  return create_services_impl(bus, std::type_identity<AppServices>{});
}

}  // namespace sil
