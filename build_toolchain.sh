#!/bin/bash
set -e

export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

mkdir -p /tmp/src && cd /tmp/src

# 1. Build Binutils
BINUTILS_VER="2.42"
wget -q https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VER}.tar.xz
tar xf binutils-${BINUTILS_VER}.tar.xz
mkdir build-binutils && cd build-binutils
../binutils-${BINUTILS_VER}/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd ..

# 2. Build GCC
GCC_VER="13.2.0"
wget -q https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.xz
tar xf gcc-${GCC_VER}.tar.xz
cd gcc-${GCC_VER}
./contrib/download_prerequisites
cd ..
mkdir build-gcc && cd build-gcc
../gcc-${GCC_VER}/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc
cd ..

# 3. Install NASM
NASM_VER="2.16.01"
wget -q https://www.nasm.us/pub/nasm/releasebuilds/${NASM_VER}/nasm-${NASM_VER}.tar.xz
tar xf nasm-${NASM_VER}.tar.xz
cd nasm-${NASM_VER}
./configure --prefix="$PREFIX"
make -j$(nproc)
make install
cd ..

echo "Cross-compiler toolchain built!"
echo "Add to PATH: export PATH=\"$PREFIX/bin:\$PATH\""
