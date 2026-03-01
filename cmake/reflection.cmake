# reflection.cmake — Isolates C++26 reflection targets from the core C++23 build.

# ── Python Generator (this target needs -freflection) ────────────────────
add_executable(generate_bindings generator/generate_bindings.cpp)
target_include_directories(generate_bindings PRIVATE generator src messages)
target_compile_options(generate_bindings PRIVATE -freflection)
target_compile_definitions(generate_bindings PRIVATE REFLECT_DOCS)
set_target_properties(generate_bindings PROPERTIES 
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    EXPORT_COMPILE_COMMANDS OFF
)

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/tests/python/reflect_pytest/generated.py ${CMAKE_SOURCE_DIR}/src/revision.h
    COMMAND generate_bindings ${CMAKE_SOURCE_DIR}/src/revision.h > ${CMAKE_SOURCE_DIR}/tests/python/reflect_pytest/generated.py
    DEPENDS generate_bindings
    COMMENT "Generating Python bindings and Revision hash using C++ reflection..."
)

add_custom_target(python_bindings ALL
    DEPENDS ${CMAKE_SOURCE_DIR}/tests/python/reflect_pytest/generated.py ${CMAKE_SOURCE_DIR}/src/revision.h
)
add_dependencies(sil_app python_bindings)

# ── Protocol Documentation Generator (also needs -freflection) ───────────────
add_executable(generate_docs generator/generate_docs.cpp)
target_include_directories(generate_docs PRIVATE generator src messages)
target_compile_options(generate_docs PRIVATE -freflection)
target_compile_definitions(generate_docs PRIVATE REFLECT_DOCS)
set_target_properties(generate_docs PROPERTIES 
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    EXPORT_COMPILE_COMMANDS OFF
)

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/doc/ipc/protocol.md ${CMAKE_SOURCE_DIR}/doc/ipc/ipc_flow.dot ${CMAKE_SOURCE_DIR}/doc/ipc/ipc_flow.svg ${CMAKE_SOURCE_DIR}/doc/ipc/ipc_flow.png
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/doc/ipc
    COMMAND generate_docs ${CMAKE_SOURCE_DIR}/doc/ipc > ${CMAKE_SOURCE_DIR}/doc/ipc/protocol.md
    COMMAND dot -Tsvg ${CMAKE_SOURCE_DIR}/doc/ipc/ipc_flow.dot -o ${CMAKE_SOURCE_DIR}/doc/ipc/ipc_flow.svg
    COMMAND dot -Tpng ${CMAKE_SOURCE_DIR}/doc/ipc/ipc_flow.dot -o ${CMAKE_SOURCE_DIR}/doc/ipc/ipc_flow.png
    DEPENDS generate_docs
    COMMENT "Generating IPC protocol documentation (Markdown, DOT, SVG, and PNG)..."
    VERBATIM
)

add_custom_target(protocol_docs ALL
    DEPENDS ${CMAKE_SOURCE_DIR}/doc/ipc/protocol.md
)
