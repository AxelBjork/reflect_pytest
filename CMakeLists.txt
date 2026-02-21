cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_COMPILER /usr/local/gcc-trunk/bin/g++)
project(reflect_pytest CXX)
set(CMAKE_CXX_STANDARD 26)  # Requires gcc trunk natively
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_RPATH /usr/local/gcc-trunk/lib64)

# ── IPC library ───────────────────────────────────────────────────────────────
add_library(ipc STATIC
    src/socket_bus.cpp
    src/message_bus.cpp
    src/udp_bridge.cpp
)
target_include_directories(ipc PUBLIC src messages)

# ── SIL application ───────────────────────────────────────────────────────────
add_executable(sil_app src/main.cpp src/logger.cpp src/simulator.cpp)
target_link_libraries(sil_app PRIVATE ipc)

# ── Python Generator (only this target needs -freflection) ────────────────────
add_executable(generate_bindings generator/generate_bindings.cpp)
target_include_directories(generate_bindings PRIVATE generator src messages)
target_compile_options(generate_bindings PRIVATE -freflection)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/python/reflect_pytest/generated.py
    COMMAND generate_bindings > ${CMAKE_BINARY_DIR}/python/reflect_pytest/generated.py
    DEPENDS generate_bindings
    COMMENT "Generating Python bindings using C++ reflection..."
)

add_custom_target(python_bindings ALL
    DEPENDS ${CMAKE_BINARY_DIR}/python/reflect_pytest/generated.py
)
add_dependencies(sil_app python_bindings)

# ── Protocol Documentation Generator (also needs -freflection) ───────────────
add_executable(generate_docs generator/generate_docs.cpp)
target_include_directories(generate_docs PRIVATE generator src messages)
target_compile_options(generate_docs PRIVATE -freflection)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/ipc_protocol.md
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/doc
    COMMAND generate_docs > ${CMAKE_BINARY_DIR}/doc/ipc_protocol.md
    DEPENDS generate_docs
    COMMENT "Generating IPC protocol Markdown documentation..."
)

add_custom_target(protocol_docs ALL
    DEPENDS ${CMAKE_BINARY_DIR}/doc/ipc_protocol.md
)

# ── GoogleTest ────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
    tests/cxx/test_placeholder.cpp
    tests/cxx/test_socket_bus.cpp
)
target_link_libraries(unit_tests PRIVATE ipc GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
