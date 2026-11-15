# 06 — Cadena de herramientas para este ARM

El televisor no es un ARM "normal": es **ARMv7 soft-float EABI5** con **glibc 2.18**. Casi todo lo
que se compile para ARM "de hoy" (hard-float, glibc 2.3x) **no va a correr**.

## El ABI, en una tabla

| Dato | Valor en este TV | Consecuencia |
|---|---|---|
| Arquitectura | ARMv7 (Cortex-A7), NEON | se puede usar `-march=armv7-a` |
| ABI de coma flotante | **soft-float** (`e_flags` bit 0x200) | compilar con `-mfloat-abi=soft` |
| `e_flags` de las libs del TV | `0x05000002` | una `.so` con `0x05000400` (hard) es rechazada por `dlopen` |
| glibc | 2.18 | un binario dinámico debe enlazarse contra **esa** libc |
| Intérprete | `/lib/ld-linux.so.3` | el linker debe dejar ese `PT_INTERP` |
| Compilador | `arm-linux-gnueabi-gcc` | produce justo ese perfil |

Comprobación rápida de un binario:

```sh
arm-linux-gnueabi-readelf -h binario | grep -E "Machine|Flags"
#   Machine: ARM
#   Flags: 0x5000200, Version5 EABI, soft-float ABI
```

## Tres formas de compilar (elegir según el caso)

### a) Herramienta suelta, sin libc (recomendado para utilidades chicas)

```sh
arm-linux-gnueabi-gcc -nostdlib -static -Os -ffreestanding \
    -march=armv7-a -mfloat-abi=soft \
    -fno-stack-protector -fno-builtin -Wl,-e,main \
    -o herramienta herramienta.c
```

- No arrastra libc: cero problemas de versiones.
- Hay que escribir `open/read/write/ioctl/mmap` como **syscalls**.
- **El número de syscall va en `r7`** (no en `r0`).
- Ojo con la división: `v / 10` hace que gcc llame a `__aeabi_uidivmod` (helper de libgcc que en
  el TV no está) y el `dlopen` falla por símbolo sin resolver. Convertir números restando
  potencias de 10.

### b) Binario estático con libc del toolchain

Funciona para programas simples (el toolchain enlaza su propia glibc), pero **no** conviene si el
programa va a cargar bibliotecas del TV: quedarían dos libc en el mismo proceso.

### c) Enlazar contra las bibliotecas del propio TV

Es lo que sirve para DirectFB: copiar `libdirectfb.so` del televisor al directorio de build y:

```sh
arm-linux-gnueabi-gcc ... -L. -ldirectfb -Wl,--allow-shlib-undefined
```

`--allow-shlib-undefined` es necesario porque la `.so` del TV tiene dependencias (png, jpeg,
freetype) que resuelve en runtime. Si la librería usa C++, hay que precargar `libstdc++`:

```sh
LD_PRELOAD=/lib/libstdc++.so.6 ./programa
```

## Dropbear (SSH con clave pública)

El firmware **no** trae `sshd`. Se compila dropbear estático desde la fuente firmada de Debian,
con el script [`../herramientas/compilar_dropbear.sh`](../herramientas/compilar_dropbear.sh), en
un contenedor `debian` amd64 con `gcc-arm-linux-gnueabi`:

```sh
./configure --host=arm-linux-gnueabi --enable-static --disable-zlib --disable-pam \
    --disable-lastlog --disable-utmp --disable-utmpx --disable-wtmp --disable-wtmpx \
    --disable-syslog CC=arm-linux-gnueabi-gcc CFLAGS="-Os -march=armv7-a" LDFLAGS="-static"
printf '#define DROPBEAR_SVR_PASSWORD_AUTH 0\n#define DROPBEAR_CLIENT 0\n' > localoptions.h
```

Detalles específicos de este equipo:

- **Sin contraseña**: el root del TV no tiene contraseña y no hay `crypt()`; solo clave pública.
- **`/etc/passwd` es de solo lectura** y el home de root es `/basic`. Solución usada:
  superponer una copia con `mount --bind`:

```sh
busybox sed 's#:/basic:#:/data/root:#' /etc/passwd > /data/passwd
busybox mount --bind /data/passwd /etc/passwd
```

- **`/dev/pts`** tiene que estar montado o la sesión SSH no puede abrir terminal:

```sh
busybox mount -t devpts devpts /dev/pts
```

El binario incluido en [`../binarios/`](../binarios/) ya está compilado y probado.

## Subir y bajar archivos

`scp` clásico **no** sirve (OpenSSH moderno usa SFTP y el TV no tiene `sftp-server`). Opciones que
funcionan:

```sh
# subir (binario-safe, por stdin)
ssh -i ~/.ssh/tv_hisense -p 2222 root@<ip> 'busybox cat > /data/archivo' < archivo

# subir por base64 (cómodo desde PowerShell)
[Convert]::ToBase64String([IO.File]::ReadAllBytes('x.bin')) |
    ssh -p 2222 root@<ip> 'busybox base64 -d > /data/x.bin'

# bajar (ojo: en PowerShell, `>` corrompe binarios)
python descargar.py /data/x.bin local.bin    # abre el destino en modo wb
```

## Probar sin el televisor: la réplica en Docker

Se puede armar una imagen ARMv7 con el rootfs volcado y correrla con QEMU:

```sh
docker run --rm -it --platform linux/arm/v7 <imagen>
```

Sirve para validar **contenido**: binarios, scripts, `.so`, incluso `dlopen` con la glibc real del
TV. **No** sirve para el arranque ni para el display (no hay SoC, ni DirectFB, ni `/dev/mtal`).

Fue clave para encontrar tres errores antes de tocar el televisor: el syscall en `r0`, el
`__aeabi_uidivmod` y el `PATH` mínimo.

## `PATH` y rutas

Los scripts que lanza el firmware heredan un `PATH` mínimo (`/usr/sbin:/usr/bin:/sbin:/bin`) y
**los applets de BusyBox no están enlazados** en `/bin`: hay que llamarlos como `busybox ls`,
`busybox ps`, o usar rutas absolutas (`/bin/busybox`). En `install.sh` conviene fijarlo:

```sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/data/bin
export PATH
```
