#!/bin/bash
set -e
echo "Cloning and building GCC from source..."
sudo apt-get update
sudo apt-get install -y flex bison build-essential tmux
if [ ! -d "gcc" ]; then
  git clone --depth 1 git://gcc.gnu.org/git/gcc.git gcc
fi
cd gcc
./contrib/download_prerequisites
mkdir -p build && cd build
../configure --disable-bootstrap --enable-languages=c,c++ --disable-multilib --prefix=/usr/local/gcc-trunk
make -j$(nproc)
sudo make install
