#pragma once

#include <tuple>
#include <type_traits>

#include "services/autonomous_service.h"
#include "services/environment_service.h"
#include "services/kinematics_service.h"
#include "services/log_service.h"
#include "services/motor_service.h"
#include "services/power_service.h"
#include "services/state_service.h"
#include "services/thermal_service.h"
#include "udp_bridge.h"

namespace sil {

/**
 * @warning CRITICAL: DO NOT MODIFY THIS FILE OR THE AppServicesContainer.
 *
 * All services must conform to the standard constructor signature:
 *   Service(ipc::MessageBus& bus)
 *
 * Inter-service communication should happen via the ipc::MessageBus to
 * maintain loose coupling and allow for easier testing/extensibility.
 * Manual wiring of dependencies is strictly prohibited.
 */

// A compile-time list of all services instantiated by the main application.
// This is used by the documentation generator to automatically reflect over
// the aggregate Subscribes/Publishes traits of the entire application.
using AppServices =
    std::tuple<MotorService, KinematicsService, PowerService, StateService, ThermalService,
               EnvironmentService, AutonomousService, LogService, ipc::UdpBridge>;

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
