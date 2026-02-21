FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# ── Base utilities ────────────────────────────────────────────────────────────
# build-essential is installed here (before the gcc-14 PPA layer) so that apt
# does not later override the update-alternatives priority we set for gcc-14.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    gnupg \
    gosu \
    lsb-release \
    software-properties-common \
    sudo \
    wget \
    && rm -rf /var/lib/apt/lists/*

# ── Ubuntu Toolchain PPA → gcc-snapshot (GCC trunk nightly) ──────────────────
# gcc-snapshot is required for C++26 static reflection (P2996), which is only
# present on GCC trunk and not in any released GCC version.
# Snapshot binaries live at /usr/lib/gcc-snapshot/bin/.
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test \
    && apt-get update && apt-get install -y --no-install-recommends \
    gcc-snapshot \
    # Register snapshot at priority 200 so it wins over build-essential's gcc-11
    && update-alternatives --install /usr/bin/gcc gcc /usr/lib/gcc-snapshot/bin/gcc 200 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/lib/gcc-snapshot/bin/g++ 200 \
    && update-alternatives --install /usr/bin/cc  cc  /usr/lib/gcc-snapshot/bin/gcc 200 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/lib/gcc-snapshot/bin/g++ 200 \
    && rm -rf /var/lib/apt/lists/*

# Make the snapshot's libstdc++ and libgcc visible to the dynamic linker
ENV PATH="/usr/lib/gcc-snapshot/bin:${PATH}" \
    LD_LIBRARY_PATH="/usr/lib/gcc-snapshot/lib64:/usr/lib/gcc-snapshot/lib:${LD_LIBRARY_PATH}"

# ── Core C++ tooling ──────────────────────────────────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    make \
    gdb \
    clang \
    clang-tidy \
    clang-format \
    cppcheck \
    valgrind \
    lcov \
    git \
    curl \
    pkg-config \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# ── Python + pytest ───────────────────────────────────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-wheel \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir \
    pytest \
    pytest-cov \
    pytest-xdist \
    pybind11

# ── Non-root user 'vscode' with sudo ─────────────────────────────────────────
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

WORKDIR /workspace
USER $USERNAME

