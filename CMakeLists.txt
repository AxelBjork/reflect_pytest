cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_COMPILER /usr/local/gcc-trunk/bin/g++)
project(reflect_pytest CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-fdiagnostics-color=always)
set(CMAKE_BUILD_RPATH /usr/local/gcc-trunk/lib64)

# ── IPC library ───────────────────────────────────────────────────────────────
add_library(ipc STATIC
    src/message_bus.cpp
    src/udp_bridge.cpp
    src/component_logger.cpp
)
target_include_directories(ipc PUBLIC src messages)

# ── SIL application ───────────────────────────────────────────────────────────
add_executable(sil_app
    src/main.cpp
    src/message_bus.cpp
    src/udp_bridge.cpp
    src/component_logger.cpp
    src/services/environment_service.cpp
    src/services/kinematics_service.cpp
    src/services/motor_service.cpp
    src/services/power_service.cpp
    src/services/state_service.cpp
    src/services/thermal_service.cpp
    src/services/autonomous_service.cpp
    src/services/log_service.cpp
    src/services/sensor_service.cpp
    src/services/revision_service.cpp
)
target_link_libraries(sil_app PRIVATE ipc)

# ── Reflection Targets (Internal C++26) ──────────────────────────────────────
include(cmake/reflection.cmake)

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
    tests/cxx/test_message_bus.cpp
)
target_link_libraries(unit_tests PRIVATE ipc GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)


# ── Options ───────────────────────────────────────────────────────────────────
option(SIL_OPTIMIZED "Build with -O3 and -flto for maximum performance" OFF)

if(SIL_OPTIMIZED)
    message(STATUS "SIL_OPTIMIZED is ON - Setting Release build type with LTO")
    set(CMAKE_BUILD_TYPE Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3 -flto -g")
    # Assembly view
    # objdump -d -C --no-show-raw-insn --no-addresses --demangle build/sil_app > build/sil_app.asm
endif()