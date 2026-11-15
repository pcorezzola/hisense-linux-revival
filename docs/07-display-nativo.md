# 07 — Display y audio nativo: el mapa completo

Esta es la parte más frustrante y la que más tiempo consumió. Queda documentada para que nadie
repita el camino.

## Cómo dibuja realmente este televisor

```
aplicaciones (navegador, Netflix, skype)
        │
   DirectFB 1.5.3  (/lib/libdirectfb.so)   ← interfaz tipo COM con vtables
        │
   libmtal.so (/basic, API MediaTek)       ← MTOSD_*, MTGFX_*, MTVDEC_*, MTAUDDEC_*
        │
   /dev/mtal  +  /dev/fb/0 (driver mt53xx)
```

## Lo que **no** funciona

| Intento | Resultado |
|---|---|
| `mmap()` sobre `/dev/fb/0` | **EINVAL (-22)**: el driver `mt53xx` no permite el mapeo clásico |
| `write()` de un frame a `/dev/fb/0` | **se bloquea en el driver (estado D) y congela el equipo** |
| `open("/dev/dsp")` (OSS) | **EINVAL (-22)** |
| `libasound` | **no existe** en el sistema (aunque hay tarjeta ALSA `NANOSICVAUDIO`) |

> **Regla de oro:** no escribir a `/dev/fb*` ni abrir `/dev/dsp` a ciegas. La única forma de
> recuperarse de un `write()` bloqueado es cortar la energía.

## DirectFB: hasta dónde llega

Probado en el equipo real, paso por paso (`dfbtest1..8` en
[`../herramientas/pruebas_dfb/`](../herramientas/pruebas_dfb/)):

| Llamada | Resultado |
|---|---|
| `DirectFBInit` | ✅ DirectFB 1.5.3, *Multi Application Core* (`/dev/fusion/0`), `/dev/fb/0` mapeado por el driver MT53 |
| `DirectFBCreate` | ✅ |
| `GetDisplayLayer(DLID_PRIMARY)` | ✅ |
| `SetCooperativeLevel` (ADMINISTRATIVE y EXCLUSIVE) | ✅ (no cambia lo siguiente) |
| `GetSurface` | ✅ 1920×1080 (960×540 en exclusivo) |
| `SetColor` | ✅ |
| `FillRectangle` | ❌ `0x192fa3b1` |
| `Flip` | ❌ `0x192fa3a9` |
| `Lock` | ❌ `0x192fa3ad` |
| `SetConfiguration` | ❌ `0x192fa3af` |
| `CreateWindow` | ⚠️ devuelve OK **pero el puntero viene nulo** |

Los códigos `0x192fa3xx` no pertenecen a la tabla estándar de `DFBResult`: son del driver.

### Las dos causas

1. **SaWMan es dueño de la capa primaria.** Una aplicación normal no dibuja sobre la capa: crea
   una **ventana** y el compositor la muestra. (`/3rd/skype/skype_ap` es una app nativa que usa
   DFB en este mismo equipo, así que el camino existe.)
2. **Desajuste de ABI.** Los headers disponibles públicamente son de la rama **1.5.0** y la
   librería es **1.5.3**. Entre esas versiones se agregaron métodos a las interfaces, así que los
   índices de vtable posteriores a cierto punto se corren — y por eso `CreateWindow` "funciona"
   pero devuelve nulo (estamos llamando a otra función).

No se encontró el tarball 1.5.3 en ningún mirror accesible (Gentoo, Buildroot, OpenWrt, Debian,
Wayback). Reconstruir las vtables desde el binario (está sin `.symtab`) es posible pero lleva
horas de ingeniería inversa.

## La API real del fabricante: `libmtal.so`

`/basic/libmtal.so` (665 KB, **2115 símbolos exportados**) es la capa que usan el navegador y
Netflix. Categorías principales:

| Prefijo | Funciones | Para qué |
|---|---|---|
| `MTAUDDEC_*` | 219 | audio |
| `MTFE_*` | 172 | front end / sintonía |
| `MTVDO_*` | 164 | video out |
| `MTVDECEX_*` / `MTVDEC_*` | 152 + 79 | decodificación de video |
| `MTDMX_*` | 134 | demux |
| **`MTOSD_*`** | **46** | **OSD / dibujo en pantalla** |
| **`MTGFX_*`** | **39** | **gráficos** |
| `MTIMAGE_*` | 40 | imágenes |
| `MTAL_Init`, `MTAL_InitEx` | — | inicialización |
| `MTAL_PLA_SetRect/SetViewRect/SetRgnVStep` | 3 | planos |

También hay `MTADAC_*` (DAC de audio: `MTADAC_SetDacFormatCfg`, `MTADAC_HwMute`,
`MTADAC_Init`, …), que es **la vía real para tener sonido** en una app nativa.

**Está sin documentar**: usar esta API implica descubrir firmas, estructuras y orden de
inicialización. Es el único camino que queda para un emulador a 60 fps con sonido.

## Resumen para decidir

| Camino | Estado |
|---|---|
| Framebuffer crudo | ❌ congela |
| DirectFB sobre la capa primaria | ❌ el driver rechaza el dibujo |
| DirectFB con ventana | ❌ no se puede pedir por el desajuste de ABI |
| `libmtal` directo | ⚠️ posible, requiere ingeniería inversa |
| Motor web (canvas) | ✅ dibuja, pero el CPU no alcanza para emular (ver [doc 08](08-experimentos-descartados.md)) |
