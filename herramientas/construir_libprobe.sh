#!/bin/sh
# Compila libprobe.so (+ libvacia.so de control) para el TV: ARMv7 soft-float, sin libc.
# Uso:  docker run --rm -v "$PWD:/w" -w /w debian:bookworm-slim sh /w/construir.sh
set -eu

apt-get update -qq
apt-get install -y -qq --no-install-recommends gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi >/dev/null

CFLAGS="-shared -fPIC -nostdlib -Os -march=armv7-a -mfloat-abi=soft \
        -fno-stack-protector -fno-asynchronous-unwind-tables"

arm-linux-gnueabi-gcc $CFLAGS -o libprobe.so probe.c -Wl,--hash-style=sysv
arm-linux-gnueabi-gcc $CFLAGS -DCTRL_VACIA -o libvacia.so probe.c -Wl,--hash-style=sysv

for f in libprobe.so libvacia.so; do
    echo "==== $f ===="
    arm-linux-gnueabi-readelf -h "$f" | grep -E "Type:|Machine:|Flags:"
    echo -n "DT_NEEDED: "; arm-linux-gnueabi-readelf -d "$f" | grep -c NEEDED || true
    echo -n "init_array: "; arm-linux-gnueabi-readelf -S "$f" | grep -c init_array || true
    echo -n "simbolos sin resolver: "; arm-linux-gnueabi-nm -D --undefined-only "$f" 2>/dev/null | wc -l
    echo -n "tamano: "; arm-linux-gnueabi-size "$f" | tail -1
done
