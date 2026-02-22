#pragma once
// common.h — Shared C++26 reflection utilities for doc generation and python bindings.

#include <array>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <string_view>

#include "messages.h"

// -------------------------------------------------------------------------------------------------
// Annotation readers
// -------------------------------------------------------------------------------------------------

template <std::meta::info R>
consteval doc::Desc get_desc() {
  for (std::meta::info a : std::meta::annotations_of(R)) {
    if (std::meta::type_of(a) == ^^doc::Desc) {
      return std::meta::extract<doc::Desc>(a);
    }
    if (std::meta::type_of(a) == ^^const doc::Desc) {
      return std::meta::extract<const doc::Desc>(a);
    }
  }
  return doc::Desc("");
}

// -------------------------------------------------------------------------------------------------
// Enum Reflection
// -------------------------------------------------------------------------------------------------

template <typename E>
consteval std::size_t get_enum_size() {
  return std::meta::enumerators_of(^^E).size();
}

template <typename E, std::size_t N>
consteval std::array<std::meta::info, N> get_enum_array() {
  auto vec = std::meta::enumerators_of(^^E);
  std::array<std::meta::info, N> arr{};
  for (std::size_t i = 0; i < N; ++i) {
    arr[i] = vec[i];
  }
  return arr;
}

template <typename E, std::size_t N>
struct EnumArrHolder {
  static constexpr auto arr = get_enum_array<E, N>();
};

// -------------------------------------------------------------------------------------------------
// Struct Reflection
// -------------------------------------------------------------------------------------------------

template <typename T>
consteval std::size_t get_fields_size() {
  auto ctx = std::meta::access_context::current();
  return std::meta::nonstatic_data_members_of(^^T, ctx).size();
}

template <typename T, std::size_t N>
consteval std::array<std::meta::info, N> get_fields_array() {
  auto ctx = std::meta::access_context::current();
  auto vec = std::meta::nonstatic_data_members_of(^^T, ctx);
  std::array<std::meta::info, N> arr{};
  for (std::size_t i = 0; i < N; ++i) {
    arr[i] = vec[i];
  }
  return arr;
}

template <typename T, std::size_t N>
struct StructArrHolder {
  static constexpr auto arr = get_fields_array<T, N>();
};
