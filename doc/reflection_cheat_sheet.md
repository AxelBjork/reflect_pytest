# C++26 Reflection Cheat Sheet & Summary

This document serves as a high-level summary and quick reference for the new static reflection features introduced in C++26. 

## Approved Papers Reference
The following foundational papers are included in C++26 related to reflection. Full HTML documents are located in this directory (`/doc`).

| Paper ID | Title | Summary |
|----------|-------|---------|
| **[P2996R13](P2996.html)** | Reflection for C++26 | The core reflection proposal. Introduces `std::meta::info`, the `^^` operator, splicers (`[: :]`), and the fundamental `<meta>` header. |
| **[P1306R5](P1306.html)**  | Expansion Statements | Introduces `template for`, enabling compile-time iteration over tuples, packs, and reflection ranges. |
| **[P3096R12](P3096.html)** | Function Parameter Reflection | Allows introspecting function parameters via reflection APIs. |
| **[P3293R3](P3293.html)**  | Splicing a base class subobject | Allows generating base class derivations programmatically. |
| **[P3394R4](P3394.html)**  | Annotations for reflection | Introduces a mechanism for user-defined attributes (annotations) that can be queried at compile-time. |
| **[P3491R3](P3491.html)**  | `define_static_` | Capability for defining static variables from reflection. |
| **[P3560R2](P3560.html)**  | Error handling in reflection | Clarifies error handling behaviors when using reflection metafunctions. |

---

## Cheat Sheet

### 1. Basic Introspection Core

C++26 reflection relies on the core opaque type `std::meta::info`, representing a handle to a program element at compile time.

**Reflecting on an entity:** Use the prefix `^^` operator.
```cpp
#include <meta>

constexpr std::meta::info class_info = ^^MyClass;
constexpr std::meta::info int_info = ^^int;
```

**Splicing (un-reflecting):** Use the `[: ... :]` syntax to convert a `std::meta::info` back into a grammatical element (like a type or an expression).
```cpp
using MyType = [: class_info :]; // Same as using MyType = MyClass;

constexpr std::meta::info val_info = ^^my_const_var;
int x = [: val_info :];
```

### 2. Common Metafunctions (`<meta>`)

Metafunctions in `std::meta` are `consteval` functions that process `std::meta::info` handles.

```cpp
namespace std::meta {
    // Queries
    consteval string_view name_of(info x);
    consteval string_view display_name_of(info x);
    consteval source_location source_location_of(info x);
    consteval bool is_type(info x);
    consteval bool is_class(info x);
    consteval bool is_function(info x);
    consteval bool is_enum(info x);

    // Retrieving members
    consteval vector<info> members_of(info x);
    consteval vector<info> nonstatic_data_members_of(info x);
    consteval vector<info> enumerators_of(info x);
    consteval vector<info> base_classes_of(info x);
}
```

### 3. Expansion Statements (`template for`)

The `template for` loop (P1306) allows you to iterate over a compile-time sequence (such as a `std::vector<std::meta::info>` returned by `members_of` or a tuple/pack) and expands the loop body **for each element at compile time**.

**Iterating over class members:**
```cpp
#include <meta>
#include <iostream>

struct Point {
    int x;
    int y;
    int z;
};

void print_members(const Point& p) {
    // Reflect on Point to get its non-static data members
    constexpr auto members = std::meta::nonstatic_data_members_of(^^Point);
    
    // Expand the loop body for each member
    template for (constexpr auto member : members) {
        std::cout << std::meta::name_of(member) << ": " 
                  << p.[:member:] << "\n";
    }
}
```

**Iterating over an Enum:**
```cpp
enum class Color { Red, Green, Blue };

void print_enum(Color c) {
    constexpr auto enumerators = std::meta::enumerators_of(^^Color);
    
    template for (constexpr auto e : enumerators) {
        if (c == [:e:]) {
            std::cout << std::meta::name_of(e) << "\n";
            return;
        }
    }
}
```

### 4. Code Generation & Proxies

You can use `template for` and splicing to automatically generate boilerplate like JSON serialization, argument parsing, or bindings.

```cpp
// Generating a tuple from a struct's members
template <typename T>
auto to_tuple(const T& obj) {
    constexpr auto members = std::meta::nonstatic_data_members_of(^^T);
    
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return std::make_tuple(obj.[: members[I] :]...);
    }(std::make_index_sequence<members.size()>{});
}
```

*(Note: with pack expansion and tuple improvements, this can potentially be simplified in C++26 further without `index_sequence`)*

### 5. Type Traits equivalent

You can perform trait checks using reflection.

```cpp
template<typename T>
consteval bool has_id_member() {
    constexpr auto members = std::meta::members_of(^^T);
    template for (constexpr auto m : members) {
        if (std::meta::name_of(m) == "id") return true;
    }
    return false;
}
```

### Summary of new syntax:
- `^^Entity` : Creates a `std::meta::info` representing `Entity`.
- `[: info_val :]` : Reifies (splices) a `std::meta::info` back into a type or value conceptually.
- `template for (constexpr auto x : range)` : Expands the loop body for each element in `range` at compile time.

---

## Compiler Support (GCC Trunk)

As of 14 Jan 2026, **GCC Trunk** has merged initial support for C++26 Reflection (`PR120775`). https://gcc.gnu.org/git/?p=gcc.git;a=commitdiff;h=4b0e94b394fa38cdc3431f3cfb333b85373bd948

**Compilation Flags Required:** 
`-std=c++26 -freflection`

**Supported Papers in this initial implementation:**
- P2996R13 (Core Reflection)
- P3394R4 (Annotations for Reflection)
- P3293R3 (Splicing a base class subobject)
- P3491R3 (`define_static_string`, `object`, `array`)
- P3096R12 (Function Parameter Reflection)

**Known Limitations / Missing Features in GCC Trunk:**
- P1306 (Expansion Statements / `template for`) is **not yet implemented** in this primary reflection merge. You cannot use `template for` natively in GCC just yet; you must rely on tuple/pack expansion techniques and generic macros/lambdas as workarounds for loop expansion.
- P3795 (Miscellaneous Reflection Cleanup) is **not yet implemented**.
