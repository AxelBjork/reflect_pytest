FROM ubuntu:rolling

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# ── Base utilities ────────────────────────────────────────────────────────────
# build-essential is installed here so that apt does not later override the 
# update-alternatives priority we set for GCC 15.
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

# ── GCC Trunk Nightly ────────────────────────────────────────────────────────
# Using ubuntu:rolling gives us access to gcc-15 natively which serves as the 
# GCC trunk nightly required for C++26 static reflection (P2996).
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-15 \
    g++-15 \
    # Register snapshot at priority 200 so it wins over build-essential's gcc
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 200 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-15 200 \
    && update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc-15 200 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-15 200 \
    && rm -rf /var/lib/apt/lists/*

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

RUN pip3 install --no-cache-dir --break-system-packages \
    pytest \
    pytest-cov \
    pytest-xdist \
    pybind11

# ── Non-root user 'vscode' with sudo ─────────────────────────────────────────
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN userdel -r ubuntu \
    && groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

WORKDIR /workspace
USER $USERNAME

