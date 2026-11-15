# 05 — Sistema de archivos y particiones

## Montajes reales

```
rootfs         /              squashfs  ro
/dev/mapper/dm-3  /3rd        squashfs  ro
/dev/mmcblk0p12   /3rd_rw     ext4      rw     (750 MB)
/dev/mmcblk0p27   /data       ext4      rw     (2 GB)
/dev/mmcblk0p10   /perm       ext4      rw     (8 MB)
none              /opt /dev/shm /var/run  tmpfs  rw
```

- `/tmp` **es un symlink a `/dev/shm/shm_tmp`** (tmpfs): nada de lo que se guarde ahí sobrevive.
- `/dev/shm` tiene 222 MB sobre RAM.
- `/perm` (8 MB) guarda estado de DRM/PlayReady.

## De dónde sale el mapa de particiones

**No hay MBR ni GPT**: la tabla viene en el `cmdline` que arma U-Boot:

```
mtdparts=mt53xx-emmc:2M(uboot),2M(uboot_env),256k(part_02),256k(part_03),
4M(kernelA),4M(kernelB),75M(rootfsA),75M(rootfsB),256k(basic),8M(perm),
320M(3rd_ro),750M(rw_area),256k(reserved),256k(channelA),...,1M(facset),...,
2048M(data)
```

Nombres que usa el fabricante en sus scripts:

| Partición | Montaje | Uso |
|---|---|---|
| `mmcblk0p7` | `/` | rootfsA (squashfs) |
| `mmcblk0p8` | — | rootfsB (respaldo) |
| `mmcblk0p11` | `/3rd` | apps (squashfs) |
| `mmcblk0p12` | `/3rd_rw` | datos de apps (ext4) |
| `mmcblk0p27` | `/data` | datos de usuario (ext4) |
| `mmcblk0p10` | `/perm` | secure storage |

## Por qué **no** se puede agrandar una partición

Son **offsets secuenciales**. Agrandar `rw_area` empuja todo lo que viene después: `logo`,
`adsp`, `pq`, `aq`, `acfg`, `facset`, `channelA/B` (canales), y **toda** la partición `data` de
2 GB. Además el `bootargs` vive en el entorno de U-Boot (`/dev/mmcblk0p2`): si queda mal, el
equipo no arranca y la única recuperación sería UART + `FlashTool` (abrir el televisor).

**Conclusión: no se reparticiona.** Si falta espacio, se libera o se usa `/data` / un USB.

## El problema típico de espacio (y su solución)

La unidad tenía `/3rd_rw` al **98%** (14 MB libres). El desglose:

| Ruta | Tamaño | Qué era |
|---|---|---|
| `/3rd_rw/internet_browser/opera_home/Cache` | **417 MB** | caché del navegador de apps |
| `/3rd_rw/iqqi/PredicDB` | 95 MB | 32 idiomas de predicción del teclado |
| `/3rd_rw/UI` | 90 MB | la interfaz (se usa) |
| `/3rd_rw/launcher` | 80 MB | el launcher (se usa) |
| `/3rd_rw/picasa`, `terratv`, `skype`, `ginga` | ~10 MB | apps muertas |

Limpiar **solo la caché** devolvió 417 MB:

```
antes:  697.5M usados / 14M libres   (98%)
después: 280.2M usados / 431.3M libres (39%)
```

El propio firmware tiene `clear_browser_cache()` en `/etc/scripts/update.sh`, así que es una
operación prevista. Es seguro hacerlo con el navegador de apps cerrado:

```sh
busybox ps | busybox grep '[i]nternet_browser' || \
  busybox rm -rf /3rd_rw/internet_browser/*/Cache/*
```

> **Ojo con `iqqi`**: parece "el teclado chino" y es tentador borrarlo, pero `libIQQILib.so` está
> mapeada por el launcher y los `tvstore` (verificable en `/proc/<pid>/maps`). Borrar la carpeta
> entera rompe el teclado en el siguiente arranque. La base de predicción sí se puede recortar,
> conservando `Spanish` y `English`.

## Qué hay en `/3rd` (solo lectura)

```
browser/            Opera Presto + lanzadores (run-Launcher, run-Browser, run-Tvstore…)
browser_engine/     libomi.so (Blink) + NSS + MediaTek
internet_browser/   lanzador de apps de internet + perfiles por app
UI/                 symlinks a /3rd_rw/UI
netflix/            app NRD 4.1.5 (PlayReady)
widevine/           libwvcdm + symlink al keybox (vacío)
dialserver/         56789/56790
launcher/, hisdk/, hisFileUtils/, tvsysinfo/…
```

## Qué hay en `/data` (nuestro territorio)

Es el lugar correcto para todo lo propio: 2 GB, ext4, y no se toca al actualizar apps.

```
/data/bin/          dropbear, dropbearkey, scp, iniciar_dropbear.sh
/data/dropbear/     clave del servidor
/data/root/.ssh/    authorized_keys
/data/hook/         copia del .pkg de arranque
/data/hook.log      registro del hook
```

## Rootfs: qué se puede leer (y qué no se debe tocar)

`/etc/inittab` define una consola serie en `ttyMT0` a 115200, y `/etc/rc.d/rc.local` tiene ramas
para `sshd` y para consola USB (`ttyUSB`, requiere `ttyUSB` en el `cmdline`). Nada de eso está
habilitado. `/sbin` no tiene `sshd` en ninguna versión conocida del firmware.

**No modificar el rootfs**: es squashfs de solo lectura y vive en la partición de mayor riesgo.
Todo lo que hace este proyecto queda en `/data` y `/3rd_rw`.
