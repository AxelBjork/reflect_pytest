#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

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

namespace detail {

// SFINAE helper to call .start() if it exists
template <typename T>
auto call_start_if_exists(T& service, int) -> decltype(service.start(), void()) {
  service.start();
}

template <typename T>
void call_start_if_exists(T&, long) {
  // No-op for services without a start() method
}

template <typename Tuple, std::size_t... Is>
void start_all_services(Tuple& services, std::index_sequence<Is...>) {
  // Pass 0 (int) to prefer the template overload that takes 'int'
  (call_start_if_exists(std::get<Is>(services), 0), ...);
}

}  // namespace detail

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
// This is also used by the documentation generator to automatically reflect over
// the aggregate Subscribes/Publishes traits of the entire application.
using AppServices =
    std::tuple<MotorService, KinematicsService, PowerService, StateService, ThermalService,
               EnvironmentService, AutonomousService, LogService, ipc::UdpBridge>;

template <typename... Ts>
struct AppServicesContainer {
  ipc::MessageBus bus;
  std::tuple<Ts...> services;

  AppServicesContainer() : bus(), services(((void)sizeof(Ts), std::ref(bus))...) {
    // Phase 1: All constructors are finished (all subscriptions registered).
    // Phase 2: Start background threads. Thread start provides memory barrier.
    detail::start_all_services(services, std::index_sequence_for<Ts...>{});
  }
};

template <typename... Ts>
AppServicesContainer<Ts...> create_services_impl(std::type_identity<std::tuple<Ts...>>) {
  return AppServicesContainer<Ts...>();
}

inline auto create_app_services() {
  return create_services_impl(std::type_identity<AppServices>{});
}

}  // namespace sil
