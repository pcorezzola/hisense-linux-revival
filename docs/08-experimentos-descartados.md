# 08 — Experimentos que no funcionaron (para no repetirlos)

Todo esto se midió en el equipo. Está acá para ahorrarle tiempo al próximo.

## 1. NES en JavaScript: la app funciona, el CPU no alcanza

Se armó la app completa (core `jsnes` + canvas + audio + teclado/gamepad + un ROM de prueba
propio), servida **por el propio televisor** con `busybox httpd`, y se abrió en el navegador de
apps vía DIAL + DevTools.

| Medición | Valor | Necesario para 60 fps |
|---|---|---|
| `nes.frame()` | **84 ms** | 16,6 ms |
| Pintado (Uint32 + `putImageData`) | 5 ms | — |
| `requestAnimationFrame` | entrega a **~6 Hz** | 60 Hz |
| Motor JS (bucle de 10M iteraciones) | 229 ms (el JIT anda) | — |

Conclusión: **el core es ~5x demasiado lento** y además `rAF` no entrega a 60 Hz (hay que usar
`setTimeout`). Un emulador en JavaScript no es viable en este procesador.

## 2. Extraer el video de YouTube (cliente propio)

Se probaron cinco clientes de InnerTube (WEB, TVHTML5, TVHTML5_SIMPLY_EMBEDDED_PLAYER,
ANDROID_VR, IOS) desde una PC:

| Cliente | Respuesta |
|---|---|
| WEB | `UNPLAYABLE` — "The page needs to be reloaded" |
| TVHTML5 | `LOGIN_REQUIRED` — "Sign in to confirm you're not a bot" |
| TVHTML5_SIMPLY_EMBEDDED_PLAYER | `ERROR` — "YouTube is no longer supported in this application" |
| ANDROID_VR | `LOGIN_REQUIRED` |
| IOS | HTTP 400 |

YouTube exige **PO token** y detección de bots. Un extractor propio no es mantenible. Lo que sí
funciona:

- **La búsqueda** por `youtubei/v1/search` (HTTP 200, 284 resultados)… pero **desde el navegador
  da 403 por CORS**. Solución: hacerla **desde el propio televisor** con `curl` (HTTP 200,
  1,75 s) y servir el JSON a la página desde el mismo origen.
- **La reproducción**: el player oficial sigue funcionando. Medido: 854×480 estable, más de 100
  segundos continuos, **0 frames dropeados**, CPU del sistema 45%.

## 3. Netflix y Prime Video

- `WIDEVINE key: NG` en el menú de fábrica **no es un error de lectura**: los directorios del
  keybox están vacíos (`/3rd_rw/widevine/keybox`, `/3rd_rw/widevine_eme/keybox`). Sin claves de
  dispositivo no hay contenido protegido.
- El motor OMI **no tiene la API EME moderna**: `navigator.requestMediaKeySystemAccess` es
  `undefined`; solo existe `webkitGenerateKeyRequest` (API de 2013).
- La app de Netflix es **NRD 4.1.5 (2015)**, basada en PlayReady, y Netflix ya no atiende
  clientes de esa generación.
- No hay runtime Android (`/system` no existe): no se pueden instalar APKs.

No hay camino dentro del televisor. La solución práctica es un dispositivo externo por HDMI.

## 4. Agrandar `/3rd_rw`

Descartado: las particiones son offsets secuenciales del `cmdline` de U-Boot. Agrandar una mueve
todas las siguientes (incluida `data` de 2 GB) y puede dejar el equipo sin arrancar. Con la caché
limpia sobra espacio (ver [doc 05](05-sistema-y-particiones.md)).

## 5. Escribir en `/tmp/fifo_am_write` para lanzar apps

Se probó mandar `:dialserver,am,:start=[youtube,-l,]` al FIFO de control de `app_man`. Resultado:
`app_man` quedó con estado inconsistente (`/tmp/am_status` = `netflix`), el televisor en **negro**
y el control remoto sin responder. Se recuperó reiniciando.

**Para lanzar apps: usar DIAL** (`POST /apps/<App>`), que es la vía soportada.

## 6. Caché y "apps inservibles"

- La caché del navegador de apps llegó a **417 MB** (el 60% de `/3rd_rw`): es lo primero que hay
  que mirar cuando falta espacio.
- `iqqi` **no** es basura: es el IME con predicción en 32 idiomas y su `lib` está mapeada por el
  launcher. Borrarlo entero rompe el teclado.

## 7. Cambiar la versión del navegador de YouTube por User-Agent

No se llegó a probar: el player no expone selector de calidad y la calidad quedó en 480p. La
hipótesis (un UA que haga que YouTube ofrezca 720p) sigue abierta; el riesgo es que sirva el sitio
de escritorio, que este motor no soporta.

## Resumen

| Idea | Veredicto |
|---|---|
| Emulador en JS | ❌ CPU |
| Emulador nativo | ⚠️ bloqueado por display (ver [doc 07](07-display-nativo.md)) |
| YouTube propio | ✅ búsqueda propia + player oficial |
| Netflix / Prime | ❌ DRM |
| Más espacio | ✅ limpiando caché |
| Apps nuevas | ✅ vía `Appinfo.json` + página propia |
