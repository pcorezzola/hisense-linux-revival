# Hisense HLE4815RTFX — Linux Revival

Reactivación de un Smart TV Hisense/BGH de 2015 con Linux embebido: acceso root, persistencia
en el arranque, herramientas propias y un mapa honesto de **qué se puede y qué no** en este
hardware. Todo lo que está acá se probó en un equipo real.

- **Modelo (etiqueta):** Hisense `HLE4815RTFX`, fabricado por BGH (Tierra del Fuego, Argentina),
  equivalencia declarada con `BLE4815RTFX`.
- **Modelo de red:** `LTDN48K390VWAM` (identificador completo `HisenseModernTVLTDN48K390VWAM`).
- **Firmware de la unidad:** `V00.01.00A.F1012` (menú de fábrica: `TN48.V00.K390.VWAM`).
- **Mainboard / chasis:** `RSAG7.820.6487` (MediaTek **MT5655**, SoC **MT5882**).
- **Fecha del trabajo:** 13 al 15 de noviembre de 2026.

---

## 1. El problema

Es un televisor de 2015 cuyo software quedó obsoleto:

- El navegador no abre la web moderna (motor de 2013-2015, sin EME, sin WebAssembly, VP9 solo
  por software).
- El catálogo de apps está muerto: la Opera TV Store cerró, Netflix no puede reproducir por
  falta de provisionamiento de Widevine, y hay decenas de apps sin utilidad (Skype, Picasa,
  TerraTV, Ginga, teclados regionales…).
- El fabricante no publica actualizaciones hace años.

La meta fue recuperar el control del equipo **sin abrirlo, sin soldar y sin flashear firmware**:
primero obtener una shell root, después hacerla persistente, y desde ahí construir lo que se
pueda (UI propia, servicios, herramientas, y evaluar emuladores y YouTube).

## 1.1 Modelos hermanos y compatibilidad (BGH y otras marcas)

Este televisor no es un producto aislado: es **una plataforma rebrandeada**. La unidad de
referencia es Hisense `HLE4815RTFX`, y su hermano directo es el **BGH `BLE4815RTFX`** (misma
placa `RSAG7.820.6487`, mismo chasis MTK5655, mismo SoC MT5882).

La evidencia más fuerte está en el propio firmware: `/basic` trae **las animaciones de arranque
de todas las marcas** que usan este chasis:

```
/basic/startupbgh/        (12 PNG)   <- BGH
/basic/startupnewsan/     (13 PNG)
/basic/startuppanavox/    (12 PNG)
/basic/startupoem/        (12 PNG)
/basic/startupkalley/     (1 PNG)
/basic/startupsharp/      (1 PNG)
```

La marca no es más que un par de variables (`/tmp/am_tv_brand`, `/tmp/am_tv_customer`) y la
animación de encendido. Todo el resto del sistema es el mismo.

### Qué debería aplicar tal cual al BGH BLE4815RTFX

- Los **puertos y servicios** (DIAL 56789/56790, MDX 9080, DevTools 9222). [doc 02](docs/02-reconocimiento-red.md)
- La **cadena de root** (`Hisense.File` + `loadLibrary` con *path traversal*). [doc 03](docs/03-conseguir-root.md)
- El **hook de persistencia** (`ModernTV_Launcher.pkg` → `appselfupgrade.sh`). [doc 04](docs/04-persistencia.md)
- El **mapa de particiones**, el stack gráfico (DirectFB 1.5.3 + `libmtal`) y la cadena de
  herramientas soft-float. [docs 05](docs/05-sistema-y-particiones.md) y [06](docs/06-cadena-de-herramientas.md)

### Qué puede diferir (verificar en cada unidad)

| Diferencia | Cómo se comprueba |
|---|---|
| Build de firmware (p. ej. `V00.01.00A.F1012` vs otras ramas como `E58.V0000.K…`) | menú de usuario y menú Factory |
| País/idioma/logo por defecto, set de apps | menú de usuario |
| **MAC, claves de Widevine/PlayReady, EEPROM del panel** | **son de cada unidad**; no se transfieren ni se copian |

Para confirmar el modelo en otra unidad: `GET http://<ip>:56790/dd.xml`, el menú Factory
(secuencia 1969) o `http://localhost:9222/json/version` desde el navegador de apps.

> Nada de esto se probó sobre una unidad BGH: la compatibilidad se deduce de la placa, del chasis
> y de que el firmware incluya sus recursos de marca. Antes de aplicar algo, confirmá el modelo
> con los tres métodos de arriba.

## 2. Qué se logró

| Objetivo | Estado |
|---|---|
| Shell root por red, sin abrir el TV | ✅ |
| SSH con clave pública que **sobrevive a los reinicios** | ✅ |
| Persistencia sin tocar el rootfs (hook de arranque del fabricante) | ✅ |
| Respaldo completo de la eMMC y del firmware oficial | ✅ |
| Recuperar espacio en `/3rd_rw` (en la unidad de referencia: 98% → 39%) | ✅ *caso concreto* |
| Mapa del stack gráfico y de audio nativo | ✅ (documentado) |
| Emuladores NES/Sega/SNES a 60 fps | ❌ *bloqueado* (ver [docs/07](docs/07-display-nativo.md)) |
| YouTube "renovado" | ⚠️ el player original funciona; búsqueda propia viable |

> **Sobre el espacio de `/3rd_rw`:** el 98% **no es una condición de fábrica**, es el estado de
> esta unidad después de usarla. El contenido del sistema ocupa ~280 MB de los 750 MB (≈39%), así
> que una unidad nueva arranca con ~430 MB libres; lo que se acumula es la **caché del navegador
> de apps**, que crecía hasta llenar la partición. La limpieza sirve en cualquier unidad que use
> YouTube/Netflix, y el hook ahora la hace automática cuando pasa los 150 MB. No es un defecto del
> modelo: es el uso.

Detalle completo del recorrido en la carpeta [`docs/`](docs/).

## 3. Hardware y sistema

| Dato | Valor |
|---|---|
| SoC | MediaTek MT5882 (plataforma MT5655) |
| CPU | 4× ARMv7 Cortex-A7, NEON, VFPv4 |
| RAM | 454 MB (con zram de 128 MB como swap) |
| Almacenamiento | eMMC ~3,9 GB |
| Kernel | Linux 3.10.27 (armv7l) |
| Userland | glibc 2.18, EABI5 **soft-float** (`e_flags 0x05000200`), BusyBox 1.22 |
| Pantalla | 1280×720, 32 bpp |
| Navegador de la interfaz | Opera Presto 2.12 (LSDK6176) |
| Navegador de apps | Opera OMI 4.2 / Blink (~Chrome 35) con DirectFB |
| Stack gráfico | DirectFB 1.5.3 + `libmtal` de MediaTek |

### Mapa de particiones (del `cmdline` del kernel)

```
uboot 2M · uboot_env 2M · kernelA 4M · kernelB 4M · rootfsA 75M · rootfsB 75M
basic 256k · perm 8M · 3rd_ro 320M · rw_area 750M · ... · data 2048M
```

- `rootfsA/B`: squashfs **solo lectura** (es donde vive `/etc`, `/bin`, `/sbin`).
- `/3rd`: squashfs solo lectura con las apps.
- `/3rd_rw`: ext4 **escribible** (750 MB) — donde el sistema guarda UI, launcher y caché.
- `/data`: ext4 **escribible** (2 GB) — el lugar seguro para todo lo propio.

> El mapa de particiones **no** está en MBR/GPT: viene en el `cmdline` que arma U-Boot
> (`mtdparts=...`). Por eso **no se puede "agrandar" una partición** sin mover todas las
> siguientes. Ver [docs/05](docs/05-sistema-y-particiones.md).

## 4. Cómo se llegó a root (resumen)

1. **Reconocimiento de red.** Escaneo TCP completo: 4 puertos abiertos → DIAL (56789/56790),
   Netflix MDX (9080), un servicio sin banner (1969). El DIAL sin autenticación permite lanzar y
   matar apps (es el **CVE-2026-123994**). Detalle: [docs/02](docs/02-reconocimiento-red.md).
2. **Firmware y respaldos.** Se consiguió el paquete oficial (`upgrade_loader.pkg`) y un volcado
   crudo de la eMMC de otra unidad. Con eso se mapeó el sistema sin tocar el televisor.
3. **DevTools del navegador de apps.** Al lanzar YouTube el navegador abre
   `--remote-debugging-port=9222`: con Chrome DevTools Protocol se leyó todo el sistema de
   archivos y se controló la pantalla.
4. **Escritura persistente.** La interfaz principal es HTML/JS en `/3rd_rw/UI/hisenseUI/`
   (escribible) y el navegador expone `Hisense.File` / `Hisense.loadLibrary`.
5. **Ejecución nativa como root.** `Hisense.loadLibrary()` arma la ruta
   `${OPERA_DIR}/jsplugins/<nombre>` **sin sanear**, así que una `.so` propia colocada en
   `/3rd_rw/appdata` se carga con *path traversal* (`../../../../3rd_rw/appdata/libx.so`). El
   constructor de esa `.so` corre como **uid 0**. Detalle: [docs/03](docs/03-conseguir-root.md).

## 5. Persistencia en el arranque (sin flashear)

`run-Launcher` ejecuta, **antes** de mostrar la interfaz:

```sh
if [ -f "/3rd_rw/upgrade/ModernTV_Launcher.pkg" ] && [ -d /3rd_rw/software-update ]; then
        sh /3rd_rw/software-update/appselfupgrade.sh
fi
```

y `appselfupgrade.sh` descomprime ese `.pkg` y corre **nuestro `install.sh` como root**, en cada
encendido. `/3rd_rw` es escribible → hook de arranque propio, sin tocar el rootfs.

Con eso, el televisor arranca con SSH (dropbear, solo clave pública), limpia la caché del
navegador si crece demasiado y deja registro en `/data/hook.log`. Detalle y código en
[docs/04](docs/04-persistencia.md) y [`herramientas/`](herramientas/).

## 6. Lo que **no** se puede (y por qué)

| Intento | Resultado |
|---|---|
| Escribir en el framebuffer (`/dev/fb/0`) | `mmap` da EINVAL y `write()` **congela el equipo** |
| Audio por OSS (`/dev/dsp`) | EINVAL; no hay `libasound` en el sistema |
| Navegador moderno (Chrome/WebView actual) | imposible: glibc 2.18, soft-float, 454 MB RAM |
| Netflix / Prime Video | sin Widevine provisionado (`keybox` vacío) y sin EME moderno |
| Emulador NES en JavaScript | medido: **84 ms por frame** (se necesitan 16,6) |
| Dibujar con DirectFB sobre la capa primaria | las operaciones de dibujo son rechazadas |

El camino nativo real existe (`MTOSD_*`/`MTGFX_*`/`MTAUDDEC_*` en `libmtal.so`) pero está **sin
documentar**. Ver [docs/07](docs/07-display-nativo.md) y [docs/08](docs/08-experimentos-descartados.md).

## 7. Lecciones que le pueden ahorrar tiempo a otro

1. **Antes de tocar nada, respaldar.** Un volcado de eMMC de otra unidad + el firmware oficial
   dan una red de seguridad; un dump de *la propia* unidad es mejor todavía.
2. **No escribir a ciegas en `/dev/fb*`.** En este SoC el framebuffer no se mapea y el `write()`
   bloquea el driver en estado D: el equipo se congela y solo se recupera cortando la energía.
3. **El ABI importa.** Es ARM soft-float EABI5: los binarios tienen que compilarse para
   `arm-linux-gnueabi` y, si son programas de bajo nivel, conviene **no depender de libc**
   (syscalls crudas) para evitar dos libcs en el mismo proceso.
4. **En ARM el número de syscall va en `r7`**, no en `r0`. Si se equivoca, el proceso muere con
   SIGILL y el síntoma no dice nada.
5. **Evitar la división en `itoa`**: gcc para ARM emite `__aeabi_uidivmod` (helper de libgcc) y
   entonces el `dlopen` falla por símbolo sin resolver. Convertir restando potencias de 10.
6. **`PATH` es mínimo** en los scripts que lanza el firmware: usar rutas absolutas.
7. **No escribir en `/tmp/fifo_am_write`**: descoloca a `app_man` y deja el televisor en negro
   sin control del remoto. Para lanzar apps, usar DIAL.
8. **La caché del navegador de apps** puede ocupar el 60% de una partición: es lo primero que
   hay que revisar cuando falta espacio.

## 8. Estructura del repositorio

```
README.md                     este archivo
docs/
  01-identificacion.md        modelo, firmware, cómo confirmarlo
  02-reconocimiento-red.md    puertos, SSDP, DIAL, el CVE
  03-conseguir-root.md        la cadena completa hasta uid 0
  04-persistencia.md          el hook de arranque y qué hace
  05-sistema-y-particiones.md mapa del almacenamiento y por qué no se reparticiona
  06-cadena-de-herramientas.md compilar para este ARM (y las trampas)
  07-display-nativo.md        DirectFB, libmtal y por qué no se pudo dibujar
  08-experimentos-descartados.md lo que probamos y no funcionó (para no repetirlo)
  09-recetas.md               comandos útiles, copiar y pegar
  10-decisiones-y-alternativas.md  cada alternativa evaluada y por qué se descartó
herramientas/                 scripts y fuentes propios
binarios/                     binarios ARM compilados (dropbear)
```

> Si vas a intentar algo, mirá primero el [doc 10](docs/10-decisiones-y-alternativas.md): están
> todas las alternativas que ya se probaron y descartaron, con el motivo concreto.

## 9. Hoja de ruta (lo que viene)

Este repositorio es la base sobre la que se apoya el resto del proyecto:

| Pieza | Estado |
|---|---|
| Acceso root + SSH con clave persistente | ✅ documentado acá |
| Panel web de control (httpd + CGIs en `/data`) | 🚧 en desarrollo |
| VNC / escritorio remoto y retransmisión de TV en vivo | 🚧 en desarrollo |
| `YoutubeNew`: búsqueda propia (InnerTube vía `curl` en el TV) + player oficial | 🚧 prototipo |
| Emuladores NES/Sega/SNES | ⏸ bloqueado por el display (ver [doc 07](docs/07-display-nativo.md)) |

La idea del panel/VNC es que el televisor deje de depender de la PC: un servicio liviano en
`/data` que arranca con el mismo hook de persistencia.

## 10. Aviso

Todo se hizo sobre un televisor **propio**, con fines de preservación y aprendizaje. Nada de lo
que hay acá modifica el firmware ni permite saltarse verificaciones de contenido: el
almacenamiento del equipo no se flasheó en ningún momento. Si vas a repetirlo, hacelo sobre tu
propio hardware y entendiendo el riesgo: un equipo mal manipulado puede quedar sin arrancar.

Los binarios de dropbear incluidos se compilaron desde la fuente firmada de Debian (licencia
MIT de Dropbear). Los scripts y textos propios de este repositorio son de uso libre; si te
sirven, citá el repo.

