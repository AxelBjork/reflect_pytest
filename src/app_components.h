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
#include "services/revision_service.h"
#include "services/sensor_service.h"
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
 * @warning: All services must conform to the standard constructor signature:
 *   Service(ipc::MessageBus& bus)
 *
 * Inter-service communication should happen via the ipc::MessageBus to
 * maintain loose coupling and allow for easier testing/extensibility.
 * Manual wiring of dependencies is strictly prohibited.
 */

// A compile-time list of all services instantiated by the main application.
// This is also used by the documentation generator to automatically reflect over
// the aggregate Subscribes/Publishes traits of the entire application.
using AppServices = std::tuple<MotorService, KinematicsService, PowerService, StateService,
                               ThermalService, EnvironmentService, AutonomousService, SensorService,
                               RevisionService, LogService, ipc::UdpBridge>;

namespace detail {

// Check if a service consumes this MsgId.
template <MsgId Target, typename Component>
constexpr bool consumes() {
  return ipc::detail::is_in_list_v<Target, typename Component::Subscribes>;
}

// Check if ANY service consumes this MsgId.
template <MsgId Target, typename Tuple>
constexpr bool has_subscribers() {
  return []<std::size_t... Is>(std::index_sequence<Is...>) {
    return (consumes<Target, std::tuple_element_t<Is, Tuple>>() || ...);
  }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Generate an inline handler that calls on_message for any matching service in the Tuple.
template <typename Tuple, MsgId target_id, std::size_t... Is>
void maybe_dispatch_to_services(void* tuple_ptr, const void* payload, std::index_sequence<Is...>) {
  auto* services = static_cast<Tuple*>(tuple_ptr);
  (..., [&]() {
    using SvcType = std::tuple_element_t<Is, Tuple>;
    if constexpr (consumes<target_id, SvcType>()) {
      auto& svc = std::get<Is>(*services);
      const auto& p = *static_cast<const typename MessageTraits<target_id>::Payload*>(payload);
      if constexpr (requires { svc.template on_message<target_id>(p); }) {
        svc.template on_message<target_id>(p);
      } else {
        svc.on_message(p);
      }
    }
  }());
}

template <MsgId I, typename Tuple>
void dispatch_for_id(void* ctx, MsgId id, const void* payload) {
  maybe_dispatch_to_services<Tuple, I>(ctx, payload,
                                       std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

static constexpr size_t K_MAX_MSG_ID = static_cast<size_t>(MsgId::RevisionResponse);

template <MsgId... Msgs1, MsgId... Msgs2>
consteval auto operator+(ipc::MsgList<Msgs1...>, ipc::MsgList<Msgs2...>) {
  return ipc::MsgList<Msgs1..., Msgs2...>{};
}

template <typename Tuple, MsgId... Ids>
consteval auto make_dispatch_table(ipc::MsgList<Ids...>) {
  std::array<ipc::MessageBus::DispatchFn, K_MAX_MSG_ID + 1> table{};
  (..., (table[static_cast<size_t>(Ids)] = &dispatch_for_id<static_cast<MsgId>(Ids), Tuple>));
  return table;
}

template <typename Tuple, std::size_t... Is>
consteval auto get_all_subscribes(std::index_sequence<Is...>) {
  return (typename std::tuple_element_t<Is, Tuple>::Subscribes{} + ...);
}

template <typename Tuple>
constexpr auto dispatch_table = make_dispatch_table<Tuple>(
    get_all_subscribes<Tuple>(std::make_index_sequence<std::tuple_size_v<Tuple>>{}));

template <typename Tuple>
void static_dispatcher(void* ctx, MsgId id, const void* payload) {
  auto index = static_cast<size_t>(id);
  if (index <= K_MAX_MSG_ID) {
    if (auto fn = dispatch_table<Tuple>[index]) {
      fn(ctx, id, payload);
    }
  }
}

}  // namespace detail

template <typename... Ts>
struct AppServicesContainer {
  ipc::MessageBus bus;
  std::tuple<Ts...> services;

  AppServicesContainer()
      : bus(&services, &detail::static_dispatcher<decltype(services)>),
        services(((void)sizeof(Ts), std::ref(bus))...) {
    // Phase 1: All constructors are finished.
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
