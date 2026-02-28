# reflect_pytest

A software-in-the-loop (SIL) test framework that uses **C++26 static reflection** to automatically expose a C++ IPC message bus to **pytest** over UDP — no hand-written glue.

## Architecture


The current POC implements a single-node Publisher/Subscriber bus backed by an AF_UNIX socket, bridged to a UDP port so `pytest` can interact with it.

```mermaid
graph LR
    subgraph Build ["Build-Time (C++)"]
        HDR["Message Headers"] --> GEN["Generator"]
        GEN -- "C++26 Reflection" --> PY["generated.py"]
        GEN -- "C++26 Reflection" --> DOC["protocol.md"]
    end

    subgraph Runtime ["Runtime (SIL Testing)"]
        SIL["sil_app (UDP Bridge)"] <-- "UDP :9000" --> TEST["pytest"]
    end

    HDR --> SIL
    PY -. "pack / unpack" .-> TEST
```

## Visual Architecture

The following diagram illustrates the complete system architecture and message flow, automatically generated from the C++ source code via reflection.

## Documentation Index

The following internal documents provide detailed information on the system's design and usage:

### 🏗️ Architecture
- **[Software Design Principles](doc/arch/design.md)**: SOA, IPC, and logging philosophy.
- **[Autonomous Service Spec](doc/arch/autonomous_service.md)**: Waypoint controller and environment adaptation.

### 🔮 Reflection System
- **[Reflection System Architecture](doc/reflection/system.md)**: Deep dive into `std::meta` pipelines and binding generation.
- **[C++26 Reflection Cheat Sheet](doc/reflection/cheat_sheet.md)**: Guide to P2996/P3394 features.

### 🔌 IPC & Protocol
- **[IPC Protocol Reference](doc/ipc/protocol.md)**: **Auto-generated** ground truth for wire formats and flows.

### 🧪 Testing
- **[SIL Testing Guide](doc/testing/sil_guide.md)**: How the Python test harness interacts with the C++ bus.

## How it works

1. All message IDs live in `enum class MsgId : uint16_t`.
2. Each ID is bound to its payload struct via `MessageTraits<MsgId::Foo>`.
3. C++26 reflection walks the enum and every struct at compile time, emitting pybind11 bindings automatically.
4. pytest sends/receives real UDP packets using the generated Python types.

## Build & Test

`pytest` is the single entry point for orchestrating the build, unit tests, and integration tests.

```bash
# 1. Build C++ app, run ctest (GTest), then run pytest SIL suite
pytest tests/python/ --build

# 2. Run the SIL suite only (app must be built)
pytest tests/python/

# 3. Stream sil_app output live to your terminal during tests
pytest tests/python/ -v -s

# 4. Simulator Mode: Run the C++ app for 10 seconds (no Python tests)
pytest tests/python/ --simulator --sim-duration 10
```

## Compiler Setup (C++26 Reflection)

This project relies on **C++26 static reflection** (P2996), which is currently only available on GCC trunk via the `-freflection` flag. 

In this devcontainer, the compiler is provided by the `gcc-snapshot` package from the `ppa:ubuntu-toolchain-r/test` Ubuntu PPA. It installs to `/usr/lib/gcc-snapshot/bin/g++`. Note that despite being trunk, the snapshot sometimes self-reports its version as `12.0.0` depending on the build date, so not all C++23 features (like `<print>`) are necessarily present in the snapshot payload.

## FAQ

**Why C++26 reflection instead of Clang-based tooling?**

Clang-based approaches (libTooling, AST matchers, Python plugins) require dissecting the AST externally — they are slow, fragile, and live outside the language. With C++26 reflection, introspection is seamless and native: normal C++ that reasons about its own types at compile time. This project is a proof-of-concept demonstrating that the result is dramatically simpler and more maintainable.

**Does the entire project need a C++26 compiler?**

No. Only **two translation units** (the binding and doc generators) are compiled with `-freflection`. The core application, the IPC library, and all GoogleTest unit tests compile and pass under **C++20**. Reflection is used purely as a side-channel to generate Python bindings and documentation — production code stays on a stable standard.

**How does reflection handle nested structs and complex types?**

The generator recursively unrolls struct fields, including fixed-size arrays and nested sub-structs, and fully handles native C++ padding via offset introspection. Template types get sanitized Python names automatically. Dynamic types are intentionally unsupported across the binary wire and trigger static assertions.

**How is the system architecture diagram generated?**

Each C++ service defines `Publishes` and `Subscribes` trait lists. The doc generator traverses these at compile time to build a complete topology graph emitted as Graphviz DOT. Reflection itself has never been the bottleneck — extracting the data is trivial. The challenge is in the visual representation: laying out clusters, aggregating edges, and producing a clean diagram from a complex message flow.

**How does pytest synchronize with the C++ application?**

The test fixtures launch the application as a subprocess and poll over UDP until it reports ready. Each test gets a fresh process and UDP client, ensuring isolation.

## Stack

| Layer | Technology |
|---|---|
| C++ standard | C++26 (`-freflection`, GCC snapshot) |
| Build | CMake + Ninja |
| IPC | AF_UNIX `SOCK_DGRAM` |
| Test transport | UDP (single port 9000) |
| Python bindings | C++26 reflection (auto-generated) |
| C++ unit tests | GoogleTest |
| SIL test runner | pytest + pytest-cov + pytest-xdist |
