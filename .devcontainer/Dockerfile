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
    git \
    && rm -rf /var/lib/apt/lists/*

# ── GCC Trunk Compilation for C++26 Reflection ───────────────────────────────
# Compile GCC from current trunk source to leverage new P2996 reflection merge
RUN apt-get update && apt-get install -y --no-install-recommends \
    flex \
    bison \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 git://gcc.gnu.org/git/gcc.git /tmp/gcc-source && \
    cd /tmp/gcc-source && \
    ./contrib/download_prerequisites && \
    mkdir build && cd build && \
    ../configure --disable-bootstrap --enable-languages=c,c++ --disable-multilib --prefix=/usr/local/gcc-trunk && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/gcc-source

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
    graphviz \
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

