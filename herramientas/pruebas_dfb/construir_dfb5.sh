#!/bin/sh
set -eu
apt-get update -qq
apt-get install -y -qq --no-install-recommends gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi libc6-dev-armel-cross >/dev/null
SRC=_dfb_src/DirectFB-directfb-1.5
CFLAGS="-nostdlib -Os -ffreestanding -march=armv7-a -mfloat-abi=soft -fno-stack-protector -fno-builtin -I$SRC/include -I$SRC/lib -Wl,-e,main -Wl,--allow-shlib-undefined"
arm-linux-gnueabi-gcc $CFLAGS -o dfbtest5 dfbtest5.c -L. -ldirectfb
arm-linux-gnueabi-readelf -d dfbtest5 | grep NEEDED
