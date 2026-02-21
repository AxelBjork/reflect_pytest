cmake_minimum_required(VERSION 3.22)
project(reflect_pytest CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── IPC library ───────────────────────────────────────────────────────────────
add_library(ipc STATIC
    src/socket_bus.cpp
    src/message_bus.cpp
    src/udp_bridge.cpp
)
target_include_directories(ipc PUBLIC include)

# ── SIL application ───────────────────────────────────────────────────────────
add_executable(sil_app src/main.cpp)
target_link_libraries(sil_app PRIVATE ipc)

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
