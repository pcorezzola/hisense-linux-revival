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

## Espacio en `/3rd_rw`: qué es de fábrica y qué se acumula

Conviene separar dos cosas, porque es fácil confundirlas:

| Concepto | Tamaño aproximado | ¿Varía entre unidades? |
|---|---|---|
| Contenido del sistema (UI, launcher, apps, IME) | **~280 MB** de 750 MB (≈39%) | No: es el mismo firmware |
| **Caché del navegador de apps** | de 0 al límite de la partición | **Sí: depende del uso** |

La partición tiene 750 MB y el sistema deja ~430 MB libres **de fábrica**. El problema no es la
plataforma: es que la **caché del navegador de apps** (el que usan YouTube, Netflix y HbbTV)
crece con cada video y **nunca se limpia sola**.

El propio fabricante lo sabe: `/etc/scripts/update.sh` tiene una función `clear_browser_cache()`
que borra `internet_browser/*/Cache/*`. Existe justamente porque la caché se desborda.

### Caso concreto: la unidad de referencia

Estaba al **98%** (14 MB libres), y el desglose lo explicaba:

| Ruta | Tamaño | Qué era |
|---|---|---|
| `/3rd_rw/internet_browser/opera_home/Cache` | **417 MB** | caché del navegador de apps |
| `/3rd_rw/iqqi/PredicDB` | 95 MB | 32 idiomas de predicción del teclado |
| `/3rd_rw/UI` | 90 MB | la interfaz (se usa) |
| `/3rd_rw/launcher` | 80 MB | el launcher (se usa) |
| `/3rd_rw/picasa`, `terratv`, `skype`, `ginga` | ~10 MB | apps muertas |

Limpiar **solo la caché** devolvió 417 MB:

```
antes:   697.5M usados /  14.0M libres  (98%)   <- estado de esa unidad, tras mucho uso
después: 280.2M usados / 431.3M libres  (39%)   <- estado de fábrica
```

> **No es un defecto del modelo.** En una unidad recién instalada, o poco usada, `/3rd_rw` está
> en ~39%. La limpieza se vuelve necesaria **a medida que se usan las apps de internet**, en
> cualquier unidad de este chasis.

### Cómo resolverlo (en cualquier unidad)

```sh
busybox ps | busybox grep '[i]nternet_browser' || \
  busybox rm -rf /3rd_rw/internet_browser/*/Cache/*
busybox df -h /3rd_rw
```

Mejor todavía: dejar la limpieza automática en el hook de arranque, con umbral, como hace
[`../herramientas/hook-install.sh`](../herramientas/hook-install.sh) (limpia si supera 150 MB y
nunca con el navegador abierto).

### Y lo que **no** hay que borrar

`iqqi` parece "el teclado chino" y es tentador, pero `libIQQILib.so` está mapeada por el launcher
y por los `tvstore` (verificable en `/proc/<pid>/maps`): borrar la carpeta entera rompe el teclado
en el siguiente arranque. La base de predicción sí se puede recortar conservando `Spanish` y
`English` (~84 MB), nunca la `lib`.

### Por qué no se reparticiona para tener más espacio

Ver [doc 07/05 arriba](#por-qué-no-se-puede-agrandar-una-partición): son offsets secuenciales del
`cmdline` de U-Boot. Con la caché limpia y el umbral automático, 430 MB alcanzan de sobra.

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

