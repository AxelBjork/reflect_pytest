#pragma once

#include <cstdint>
#include <iostream>
#include <meta>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>

#include "autonomous_msgs.h"
#include "common.h"
#include "core_msgs.h"
#include "simulation_msgs.h"

using namespace std::string_view_literals;

// -------------------------------------------------------------------------------------------------
// Enum Reflection
// -------------------------------------------------------------------------------------------------

template <typename E>
void generate_enum() {
  std::cout << "class " << std::meta::identifier_of(^^E) << "(IntEnum):\n";
  constexpr auto desc = get_desc<^^E>();
  if (desc.text[0] != '\0') {
    std::cout << "    \"\"\"" << desc.text << "\"\"\"\n";
  }
  constexpr std::size_t N = get_enum_size<E>();
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., [] {
      constexpr auto e = EnumArrHolder<E, N>::arr[Is];
      std::cout << "    " << std::meta::identifier_of(e) << " = " << static_cast<uint16_t>([:e:])
                << "\n";
    }());
  }(std::make_index_sequence<N>{});
  std::cout << "\n";
}

// -------------------------------------------------------------------------------------------------
// Struct Reflection
// -------------------------------------------------------------------------------------------------

// Python type mapping
template <std::meta::info Type>
consteval std::string_view get_python_type_hint() {
  using T = typename[:Type:];
  if constexpr (std::is_same_v<T, bool>) {
    return "bool"sv;
  } else if constexpr (std::is_integral_v<T>) {
    return "int"sv;
  } else if constexpr (std::is_floating_point_v<T>) {
    return "float"sv;
  } else if constexpr (std::is_enum_v<T>) {
    return std::meta::identifier_of(Type);
  } else if constexpr (std::is_array_v<T>) {
    return "bytes"sv;  // our arrays are char text[...], mapped to bytes
  }
  return "Any"sv;
}

template <std::meta::info Type>
consteval std::string_view get_struct_format_char() {
  using T = typename[:Type:];
  if constexpr (std::is_same_v<T, bool>)
    return "?"sv;
  else if constexpr (std::is_integral_v<T>) {
    if constexpr (sizeof(T) == 1) return std::is_signed_v<T> ? "b"sv : "B"sv;
    if constexpr (sizeof(T) == 2) return std::is_signed_v<T> ? "h"sv : "H"sv;
    if constexpr (sizeof(T) == 4) return std::is_signed_v<T> ? "i"sv : "I"sv;
    if constexpr (sizeof(T) == 8) return std::is_signed_v<T> ? "q"sv : "Q"sv;
  } else if constexpr (std::is_floating_point_v<T>) {
    if constexpr (sizeof(T) == 4) return "f"sv;
    if constexpr (sizeof(T) == 8) return "d"sv;
  } else if constexpr (std::is_enum_v<T>) {
    if constexpr (sizeof(T) == 1) return "B"sv;
    if constexpr (sizeof(T) == 2) return "H"sv;
    if constexpr (sizeof(T) == 8) return "Q"sv;
  }
  return "?"sv;
}

// Helper to detect generic type templates such as std::array
template <typename>
struct is_std_array : std::false_type {};
template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
void generate_struct(std::set<std::string>& visited) {
  std::string class_name = get_python_type_name<T>();
  if (visited.count(class_name)) return;
  visited.insert(class_name);

  constexpr std::size_t N = get_fields_size<T>();

  // 1. Recursive generation of nested non-POD structs
  if constexpr (N > 0) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (..., [&] {
        constexpr auto field = StructArrHolder<T, N>::arr[Is];
        constexpr auto type = std::meta::type_of(field);
        using FT = typename[:type:];
        using BaseT = std::remove_all_extents_t<FT>;

        if constexpr (is_std_array<BaseT>::value) {
          using ElemT = typename BaseT::value_type;
          if constexpr (std::is_class_v<ElemT> && !is_std_array<ElemT>::value) {
            generate_struct<ElemT>(visited);
          }
        } else if constexpr (std::is_class_v<BaseT>) {
          generate_struct<BaseT>(visited);
        }
      }());
    }(std::make_index_sequence<N>{});
  }

  // 2. Class Definition
  std::cout << "@dataclass\nclass " << class_name << ":\n";

  constexpr auto desc = get_desc<^^T>();
  if (desc.text[0] != '\0') {
    std::cout << "    \"\"\"" << desc.text << "\"\"\"\n";
  }
  std::cout << "    WIRE_SIZE = " << sizeof(T) << "\n";

  std::string pack_args;
  std::string pack_fmt = "<";
  std::string pack_instructions = "        data = bytearray()\n";
  std::string unpack_instructions = "        offset = 0\n";
  std::string unpack_args;
  std::size_t current_offset = 0;

  auto flush_format = [&]() {
    if (pack_fmt != "<") {
      pack_instructions +=
          "        data.extend(struct.pack(\"" + pack_fmt + "\", " + pack_args + "))\n";
      // If there is only one variable being unpacked, struct.unpack_from returns a 1-element tuple,
      // so we must extract it.
      bool is_single_val = (unpack_args.find(',') == std::string::npos);
      if (is_single_val) {
        unpack_instructions += "        " + unpack_args + " = struct.unpack_from(\"" + pack_fmt +
                               "\", data, offset)[0]\n";
      } else {
        unpack_instructions += "        " + unpack_args + " = struct.unpack_from(\"" + pack_fmt +
                               "\", data, offset)\n";
      }
      unpack_instructions += "        offset += struct.calcsize(\"" + pack_fmt + "\")\n";
      pack_fmt = "<";
      pack_args = "";
      unpack_args = "";
    }
  };

  if constexpr (N > 0) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (..., [&] {
        constexpr auto field = StructArrHolder<T, N>::arr[Is];
        constexpr auto type = std::meta::type_of(field);
        using FT = typename[:type:];

        std::string field_name{std::meta::identifier_of(field)};
        constexpr auto field_desc = get_desc<field>();

        // calculate padding
        constexpr auto off = std::meta::offset_of(field);
        constexpr std::size_t field_offset = off.bytes;

        // Type Hint & Docstring
        if constexpr (is_std_array<FT>::value) {
          using ElemT = typename FT::value_type;
          std::string elem_type = get_python_type_name<ElemT>();
          if constexpr (std::is_same_v<ElemT, uint8_t> || std::is_same_v<ElemT, char> ||
                        std::is_same_v<ElemT, int8_t>) {
            std::cout << "    " << field_name << ": bytes\n";
          } else {
            std::cout << "    " << field_name << ": list[" << elem_type << "]\n";
          }
        } else if constexpr (std::is_array_v<FT>) {
          using ElemT = std::remove_all_extents_t<FT>;
          std::string elem_type = get_python_type_name<ElemT>();
          if constexpr (std::is_same_v<ElemT, uint8_t> || std::is_same_v<ElemT, char> ||
                        std::is_same_v<ElemT, int8_t>) {
            std::cout << "    " << field_name << ": bytes\n";
          } else {
            std::cout << "    " << field_name << ": list[" << elem_type << "]\n";
          }
        } else if constexpr (std::is_class_v<FT>) {
          std::cout << "    " << field_name << ": " << get_python_type_name<FT>() << "\n";
        } else {
          std::cout << "    " << field_name << ": " << get_python_type_hint<type>() << "\n";
        }

        if (field_desc.text[0] != '\0') {
          std::cout << "    \"\"\"" << field_desc.text << "\"\"\"\n";
        }

        // Handle Padding
        if (field_offset > current_offset) {
          std::size_t pad = field_offset - current_offset;
          pack_fmt += std::to_string(pad) + "x";
          current_offset += pad;
        }

        // Pack & Unpack Generation logic
        if constexpr (is_std_array<FT>::value) {
          using ElemT = typename FT::value_type;
          constexpr auto N_elems = std::tuple_size_v<FT>;

          if constexpr (std::is_same_v<ElemT, uint8_t> || std::is_same_v<ElemT, char> ||
                        std::is_same_v<ElemT, int8_t>) {
            // Raw bytes array
            pack_fmt += std::to_string(N_elems) + "s";
            if (!pack_args.empty()) pack_args += ", ";
            pack_args += "self." + field_name;

            if (!unpack_args.empty()) unpack_args += ", ";
            unpack_args += field_name;
            current_offset += N_elems;
          } else {
            flush_format();
            // Complex Struct Array
            std::string sub_type = get_python_type_name<ElemT>();
            pack_instructions += "        for item in self." + field_name + ":\n";
            pack_instructions += "            if not hasattr(item, 'pack_wire'):\n";
            pack_instructions += "                if isinstance(item, tuple):\n";
            pack_instructions += "                    item = " + sub_type + "(*item)\n";
            pack_instructions += "                elif isinstance(item, dict):\n";
            pack_instructions += "                    item = " + sub_type + "(**item)\n";
            pack_instructions += "                else:\n";
            pack_instructions += "                    item = " + sub_type + "(item)\n";
            pack_instructions += "            data.extend(item.pack_wire())\n";

            unpack_instructions += "        " + field_name + " = []\n";
            unpack_instructions += "        for _ in range(" + std::to_string(N_elems) + "):\n";
            unpack_instructions += "            sub_size = " + sub_type + ".WIRE_SIZE\n";
            unpack_instructions +=
                "            item = " + sub_type + ".unpack_wire(data[offset:offset+sub_size])\n";
            unpack_instructions += "            " + field_name + ".append(item)\n";
            unpack_instructions += "            offset += sub_size\n";
            current_offset += (N_elems * sizeof(ElemT));
          }
        } else if constexpr (std::is_array_v<FT>) {
          using ElemT = std::remove_all_extents_t<FT>;
          constexpr auto N_elems = std::extent_v<FT>;

          if constexpr (std::is_same_v<ElemT, uint8_t> || std::is_same_v<ElemT, char> ||
                        std::is_same_v<ElemT, int8_t>) {
            // Raw bytes array
            pack_fmt += std::to_string(N_elems) + "s";
            if (!pack_args.empty()) pack_args += ", ";
            pack_args += "self." + field_name;

            if (!unpack_args.empty()) unpack_args += ", ";
            unpack_args += field_name;
            current_offset += N_elems;
          } else {
            flush_format();
            // Complex Struct Array
            std::string sub_type = get_python_type_name<ElemT>();
            pack_instructions +=
                "        if isinstance(self." + field_name + ", (bytes, bytearray)):\n";
            pack_instructions += "            data.extend(self." + field_name + ")\n";
            pack_instructions += "        else:\n";
            pack_instructions += "            for item in self." + field_name + ":\n";
            pack_instructions += "                if not hasattr(item, 'pack_wire'):\n";
            pack_instructions += "                    if isinstance(item, tuple):\n";
            pack_instructions += "                        item = " + sub_type + "(*item)\n";
            pack_instructions += "                    elif isinstance(item, dict):\n";
            pack_instructions += "                        item = " + sub_type + "(**item)\n";
            pack_instructions += "                    else:\n";
            pack_instructions += "                        item = " + sub_type + "(item)\n";
            pack_instructions += "                data.extend(item.pack_wire())\n";

            unpack_instructions += "        " + field_name + " = []\n";
            unpack_instructions += "        for _ in range(" + std::to_string(N_elems) + "):\n";
            unpack_instructions += "            sub_size = " + sub_type + ".WIRE_SIZE\n";
            unpack_instructions +=
                "            item = " + sub_type + ".unpack_wire(data[offset:offset+sub_size])\n";
            unpack_instructions += "            " + field_name + ".append(item)\n";
            unpack_instructions += "            offset += sub_size\n";
            current_offset += (N_elems * sizeof(ElemT));
          }
        } else if constexpr (std::is_class_v<FT>) {
          flush_format();
          std::string sub_type = get_python_type_name<FT>();
          pack_instructions += "        data.extend(self." + field_name + ".pack_wire())\n";

          unpack_instructions += "        sub_size = " + sub_type + ".WIRE_SIZE\n";
          unpack_instructions += "        " + field_name + " = " + sub_type +
                                 ".unpack_wire(data[offset:offset+sub_size])\n";
          unpack_instructions += "        offset += sub_size\n";
          current_offset += sizeof(FT);
        } else {
          // POD Primitives
          pack_fmt += get_struct_format_char<type>();
          if (!pack_args.empty()) pack_args += ", ";
          pack_args += "self." + field_name;

          if (!unpack_args.empty()) unpack_args += ", ";
          unpack_args += field_name;
          current_offset += sizeof(FT);
        }
      }());
    }(std::make_index_sequence<N>{});

    // Handle trailing padding
    if (sizeof(T) > current_offset) {
      std::size_t pad = sizeof(T) - current_offset;
      pack_fmt += std::to_string(pad) + "x";
      current_offset += pad;
    }

    flush_format();
  } else {
    std::cout << "    pass\n";
    pack_instructions += "        pass\n";
    unpack_instructions += "        pass\n";
  }

  std::cout << "\n";
  std::cout << "    def pack_wire(self) -> bytes:\n";
  if (N > 0) {
    std::cout << pack_instructions;
    std::cout << "        return bytes(data)\n\n";
  } else {
    std::cout << "        return b\"\"\n\n";
  }

  std::cout << "    @classmethod\n";
  std::cout << "    def unpack_wire(cls, data: bytes) -> \"" << class_name << "\":\n";
  if (N > 0) {
    std::cout << unpack_instructions;
    std::cout << "        return cls(";

    // Build return cls constructor args
    bool first = true;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (..., [&] {
        constexpr auto field = StructArrHolder<T, N>::arr[Is];
        std::string field_name{std::meta::identifier_of(field)};
        if (!first) std::cout << ", ";
        std::cout << field_name << "=" << field_name;
        first = false;
      }());
    }(std::make_index_sequence<N>{});

    std::cout << ")\n\n";
  } else {
    std::cout << "        return cls()\n\n";
  }
}

template <uint32_t Id, typename = void>
struct payload_or_void {
  using type = void;
};

template <uint32_t Id>
struct payload_or_void<Id, std::void_t<typename MessageTraits<static_cast<MsgId>(Id)>::Payload>> {
  using type = typename MessageTraits<static_cast<MsgId>(Id)>::Payload;
};

template <uint32_t Id>
void generate_struct_for_msg_id(std::set<std::string>& visited) {
  using T = typename payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    generate_struct<T>(visited);
  }
}

template <typename ActualT, uint32_t Id>
void emit_metadata_for_actual_type() {
  std::string class_name = get_python_type_name<ActualT>();
  std::cout << "    MsgId." << MessageTraits<static_cast<MsgId>(Id)>::name << ": " << class_name
            << ",\n";
}

template <uint32_t Id>
void emit_metadata_for_msg_id() {
  using T = typename payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    emit_metadata_for_actual_type<T, Id>();
  }
}

template <uint32_t Id>
void emit_size_for_msg_id() {
  using T = typename payload_or_void<Id>::type;
  if constexpr (!std::is_void_v<T>) {
    std::cout << "    MsgId." << MessageTraits<static_cast<MsgId>(Id)>::name << ": " << sizeof(T)
              << ",\n";
  }
}
