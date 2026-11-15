# 10 — Alternativas evaluadas y por qué se descartaron

Cada decisión de este proyecto tuvo alternativas. Están todas acá, con el motivo concreto del
descarte y la evidencia que lo respalda. Sirve para no repetir caminos ya cerrados.

---

## A. Cómo conseguir root

| Alternativa | Por qué se descartó |
|---|---|
| **UART física** (`ttyMT0`, 115200 8N1) | Requiere **abrir el televisor** y conectar un adaptador USB-serie 3,3 V. El manual de servicio confirma el procedimiento y el pinout, pero la decisión del dueño fue no abrir el equipo. Queda como plan de recuperación. |
| **Flashear un rootfs modificado** (con `sshd` en `/sbin`, que `rc.local` arrancaría solo) | No se sabe si el actualizador verifica el paquete **antes** de escribir; el rootfs **no tiene recuperación automática** (si se corrompe, no arranca); y al principio no había un volcado propio validado. Riesgo de brick alto para un beneficio que terminó consiguiéndose por software. |
| **Exploit de memoria en `dialserver`** (stack overflow pre-auth en 56789) | Confirmado como **DoS remoto**: se controla el `saved LR` pero `r0` no apunta al payload y ASLR no se puede evadir sin un *leak*. No se encontró gadget `r0 = sp ± N` ni forma de saltar a `system()`. |
| **Inyección de comandos en `app_man`** (`delete` → `rm -rf %s`) | El FIFO recibe los argumentos **URL-encoded** (todo salvo `[A-Za-z0-9-_.~]`), y el parser no decodifica antes de parsear. No alcanzable desde DIAL. |
| **Buscar un CVE del navegador Opera** (SDK 3.6 / LSDK6176) | Búsqueda dirigida sin resultados: no hay CVE ni PoC público para esa versión embebida. La cadena UA "iPad/WebKit 534.46" es un disfraz y no sirve para elegir exploits de Safari. |
| ✅ **Elegida: la cadena del navegador** (`Hisense.File` + `Hisense.loadLibrary` con *path traversal*) | Usa **APIs que el firmware expone a propósito**, sin corrupción de memoria, y el proceso del navegador corre como **root**. Reproducible y reversible. Ver [doc 03](03-conseguir-root.md). |

---

## B. Cómo hacerlo persistente

| Alternativa | Por qué se descartó |
|---|---|
| Modificar el rootfs squashfs | Implica reflashear (mismo riesgo que A). |
| `bind-mount` en caliente de `rc.local` o `autorun.sh` | Se pierde al reiniciar… y para rehacerlo al arrancar hace falta **otro** mecanismo de arranque: circular. |
| Banderas `MANUAL_LAUNCH` / `MANUAL_3RD` | Solo **desactivan** el autoarranque; no ejecutan scripts propios. |
| `userjs` del launcher + `Hisense.loadLibrary` | Viable en teoría, pero depende de que el plugin exponga `loadLibrary` **en el proceso del launcher** (no verificado) y su `OPERA_DIR` es de solo lectura. El hook del `.pkg` es shell puro, verificable en la réplica Docker. |
| Escribir el flag `170` en `/dev/mmcblk0p6` para que `update.sh` reflashee desde `/data` | Funciona, pero **reflashea el rootfs**: riesgo innecesario cuando ya había una vía sin escritura. |
| ✅ **Elegida: `ModernTV_Launcher.pkg`** (`appselfupgrade.sh`) | Es el **auto-actualizador del propio fabricante**: corre antes de la interfaz, como root, desde una partición escribible (`/3rd_rw`). No toca el rootfs y se revierte borrando un archivo. Ver [doc 04](04-persistencia.md). |

---

## C. Cómo resolver la falta de espacio en `/3rd_rw`

| Alternativa | Por qué se descartó |
|---|---|
| **Agrandar la partición** | La tabla viene en el `cmdline` de U-Boot con offsets **secuenciales**: agrandar `rw_area` empuja `logo`, `adsp`, `pq`, `aq`, `facset`, `channelA/B` y la partición `data` de 2 GB. Perder configuración de fábrica y arriesgar el arranque, sin recuperación sencilla. |
| **USB fijo + bind-mount de la caché** | Viable, pero agrega hardware y depende de que el puerto tenga energía; además el kernel **no tiene `cifs`/`nfs`**, así que tampoco sirve para montar un share del servidor. Queda como opción si en el futuro hace falta más espacio para medios. |
| Borrar "apps inservibles" | Impacto bajo (`picasa`+`terratv`+`skype`+`ginga` ≈ 10 MB) y riesgo de romper referencias del launcher. |
| Borrar `iqqi` entero | **No**: es el IME con predicción en 32 idiomas y su `lib` está mapeada por el launcher y los `tvstore` (visible en `/proc/<pid>/maps`). Borrarlo rompe el teclado. |
| ✅ **Elegida: limpiar la caché del navegador** (417 MB en la unidad de referencia) + umbral automático de 150 MB en el hook | El propio firmware prevé esta limpieza (`clear_browser_cache()` en `update.sh`). Ojo: **no es un defecto del modelo** — de fábrica la partición está al ~39%; la caché crece con el uso de YouTube/Netflix. Devolvió 417 MB y no rompe nada. |

---

## D. Cómo dibujar desde una app nativa

| Alternativa | Por qué se descartó |
|---|---|
| **Framebuffer crudo** (`mmap` / `write` sobre `/dev/fb/0`) | `mmap` devuelve **EINVAL**; `write` **bloquea el driver en estado D y congela el televisor**. Solo se recupera cortando la energía. |
| **DirectFB sobre la capa primaria** | La inicialización anda, pero `FillRectangle`, `Flip` y `Lock` son **rechazados por el driver** (`0x192fa3xx`) en los dos niveles de cooperación. |
| **DirectFB con ventana** (`CreateWindow`) | Devuelve OK **con puntero nulo**: nuestros headers son de la rama **1.5.0** y la librería es **1.5.3**, así que los índices de vtable se corren. No se encontró el tarball 1.5.3 público (Gentoo, Buildroot, OpenWrt, Debian, Wayback). |
| **`libmtal` directo** (`MTOSD_*`, `MTGFX_*`) | Es **el camino real** (46 + 39 funciones, la misma capa que usa Netflix), pero está **sin documentar**: requiere ingeniería inversa de firmas y estructuras. **No descartado: pendiente.** |
| ✅ **Elegida: motor web (canvas) para la interfaz** | Dibuja sin pelear con el compositor. Sirve para UI, **no** para emular (ver E). |

Detalle completo en [doc 07](07-display-nativo.md).

---

## E. Cómo correr emuladores

| Alternativa | Por qué se descartó |
|---|---|
| **Nativo con DirectFB** | Bloqueado por D: no hay forma de obtener una superficie donde dibujar con los headers disponibles. |
| **JavaScript en el navegador** (`jsnes` + canvas) | Medido en el equipo: **84 ms por frame** (se necesitan 16,6) y `requestAnimationFrame` entrega a **~6 Hz**. El pintado es barato (5 ms): el cuello es la emulación. 5x demasiado lento. |
| **Streaming desde la PC o un servidor** | El requisito del proyecto era **no depender de la PC**; además el Wi-Fi del televisor da 0,5-1 MB/s, insuficiente para video de juego. |
| **Portar un navegador moderno** para usar emuladores WASM | Imposible: glibc 2.18, soft-float, 454 MB de RAM, sin WASM en el motor actual. |
| ⏸ **Pendiente: core nativo + `libmtal`** | Es la única vía a 60 fps. Requiere primero resolver el display (D). |

Detalle en [doc 07](07-display-nativo.md) y [doc 08](08-experimentos-descartados.md).

---

## F. Cómo "actualizar" YouTube

| Alternativa | Por qué se descartó |
|---|---|
| **Extractor propio** (InnerTube desde la app) | Probados **5 clientes** (WEB, TVHTML5, TVHTML5_SIMPLY_EMBEDDED_PLAYER, ANDROID_VR, IOS): `UNPLAYABLE`, `LOGIN_REQUIRED` ("confirm you're not a bot"), cliente discontinuado y HTTP 400. YouTube exige **PO token**; mantener un extractor propio es inviable. |
| **yt-dlp en la PC** | Dependencia externa (requisito: sin la PC). |
| **Instancia pública de Piped/Invidious** | Desde el navegador dio **403**; las instancias públicas son inestables y añaden un tercero en el medio. |
| **App nativa nueva** (Android TV) | No hay runtime Android (`/system` no existe). |
| ✅ **Elegida: UI propia + player oficial** | La búsqueda se hace **en el propio televisor** con `curl` a `youtubei/v1/search` (HTTP 200, 284 resultados, 1,75 s) y se sirve por el httpd local (mismo origen, sin CORS). La reproducción se delega al player oficial, que sigue funcionando (854×480 estable, >100 s, 0 frames dropeados). |
| ⏸ **Pendiente: forzar 720p** | El player no expone selector de calidad; la palanca sería un User-Agent distinto, con el riesgo de que sirva el sitio de escritorio (que este motor no soporta). |

---

## G. Netflix y Prime Video

| Alternativa | Por qué se descartó |
|---|---|
| Instalar una app nueva (APK) | No hay runtime Android. |
| Web con DRM | `navigator.requestMediaKeySystemAccess` es **undefined** (solo existe la API EME de 2013) y el **keybox de Widevine está vacío** (`keybox` vacío en `/3rd_rw/widevine*`), coherente con `WIDEVINE key: NG` del menú de fábrica. |
| Actualizar la app de Netflix | Es **NRD 4.1.5 (2015)**, basada en PlayReady, fuera de soporte por Netflix; no hay cliente para esta plataforma. |
| ✅ **Recomendación: dispositivo externo por HDMI** | El televisor es un buen panel; el límite es el SoC y el provisionamiento de DRM. |

---

## H. Modificar el firmware en general

Se evaluó y **se descartó por completo**: sin certeza sobre la verificación del paquete, sin
recuperación automática del rootfs, y con el objetivo (root + persistencia) ya resuelto por
software. El almacenamiento del equipo **nunca se flasheó**, y por eso todas las pruebas de este
proyecto son reversibles: borrar un archivo en `/data` o `/3rd_rw` deshace todo.

---

## Resumen de decisiones

| Decisión | Elegida | Descartadas |
|---|---|---|
| root | cadena del navegador | UART, flasheo, `dialserver`, `app_man`, CVE del navegador |
| persistencia | `ModernTV_Launcher.pkg` | modificar rootfs, bind-mount, `MANUAL_*`, `userjs`, flag 170 |
| espacio | limpiar caché | reparticionar, USB, borrar apps, borrar `iqqi` |
| display | motor web | fb crudo, DFB capa, DFB ventana (*pendiente*: `libmtal`) |
| emuladores | *pendiente* (`libmtal`) | nativo DFB, JS, streaming, navegador moderno |
| YouTube | UI propia + player oficial | extractor propio, yt-dlp, Piped, app Android |
| Netflix/Prime | dispositivo externo | APK, web DRM, actualizar NRD |
| firmware | no tocar | (todas las variantes de flasheo) |

