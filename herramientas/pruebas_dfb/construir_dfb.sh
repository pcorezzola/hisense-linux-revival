#!/bin/sh
# Compila dfbtest1/2/3 contra los headers de DirectFB 1.5 y el libdirectfb.so del TV.
# Uso:  docker run --rm -v "$PWD:/w" -w /w debian:bookworm-slim sh construir_dfb.sh
set -eu
apt-get update -qq
apt-get install -y -qq --no-install-recommends gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi libc6-dev-armel-cross >/dev/null

SRC=_dfb_src/DirectFB-directfb-1.5
CFLAGS="-nostdlib -Os -ffreestanding -march=armv7-a -mfloat-abi=soft \
        -fno-stack-protector -fno-builtin -I$SRC/include -I$SRC/lib -Wl,-e,main -Wl,--allow-shlib-undefined"

for p in 1 2 3; do
    arm-linux-gnueabi-gcc $CFLAGS -DPASO=$p -o dfbtest$p dfbtest.c -L. -ldirectfb
    echo "==== dfbtest$p ===="
    arm-linux-gnueabi-readelf -d dfbtest$p | grep -E "NEEDED|INTERP" || true
    arm-linux-gnueabi-nm -u dfbtest$p | head -5
    arm-linux-gnueabi-size dfbtest$p | tail -1
done

