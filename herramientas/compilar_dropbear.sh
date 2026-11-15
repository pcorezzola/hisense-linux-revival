#!/bin/sh
# Corre en un contenedor debian amd64 (lo lanza compilar_dropbear.py / el comando del README).
# Compila dropbear ESTÁTICO para ARMv7 soft-float (armel), el ABI del TV (ld-linux.so.3, glibc 2.18).
set -eu
sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/debian.sources
apt-get update -qq
apt-get install -y -qq --no-install-recommends gcc-arm-linux-gnueabi libc6-dev-armel-cross make dpkg-dev ca-certificates >/dev/null
cd /tmp && apt-get source -qq dropbear >/dev/null     # fuente firmada por Debian
cd dropbear-*/
./configure --host=arm-linux-gnueabi --enable-static --disable-zlib --disable-pam --disable-lastlog \
  --disable-utmp --disable-utmpx --disable-wtmp --disable-wtmpx --disable-syslog \
  CC=arm-linux-gnueabi-gcc CFLAGS="-Os -march=armv7-a" LDFLAGS="-static" >/dev/null
# solo claves publicas: sin contrasena (el root del TV no tiene) y sin depender de crypt()
printf '#define DROPBEAR_SVR_PASSWORD_AUTH 0\n#define DROPBEAR_CLIENT 0\n' > localoptions.h
make PROGRAMS="dropbear dropbearkey scp" STATIC=1 > /tmp/make.log 2>&1 || { grep -m5 -B2 -A4 "error" /tmp/make.log; exit 1; }
arm-linux-gnueabi-strip dropbear dropbearkey scp
cp dropbear dropbearkey scp /out/
grep -m1 '^#define DROPBEAR_VERSION' src/*.h sysoptions.h 2>/dev/null | head -1 || true
ls -l /out
