#!/bin/bash

cd musl
CROSS_COMPILE=arm-linux-gnueabihf- ./configure --target=aarch64-linux-gnu --disable-shared
make clean
make -j
cd ../

cd rpi_ws281x
scons -Q TOOLCHAIN=arm-linux-gnueabihf STATIC_LIBC_ROOT=../musl/lib/
cd ../
