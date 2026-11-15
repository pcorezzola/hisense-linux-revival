# 01 — Identificación del equipo

Antes de tocar nada hay que saber **exactamente** qué equipo es. En estos televisores el nombre
comercial de la etiqueta, el nombre de red y el modelo de la placa no coinciden entre sí.

## Datos de la unidad de referencia

| Dato | Valor | De dónde sale |
|---|---|---|
| Marca | Hisense | etiqueta trasera |
| Modelo comercial | **HLE4815RTFX** | etiqueta trasera |
| Fabricante | BGH S.A., Tierra del Fuego (Argentina) | etiqueta |
| Equivalencia declarada | BLE4815RTFX | documentación técnica / foros |
| Nombre de red | `HisenseModernTVLTDN48K390VWAM` | SSDP / DHCP |
| Modelo en red | `LTDN48K390VWAM` | `dd.xml` del DIAL |
| Software | `V00.01.00A.F1012` | menú de usuario |
| Versión de fábrica | `TN48.V00.K390.VWAM` / `01.00A.F1012` | menú Factory |
| Mainboard | `RSAG7.820.6487` | manual de servicio |
| SoC | MediaTek **MT5882** (plataforma MT5655) | `/proc/cpuinfo` (`Hardware: MT5882`) |
| MAC | la de la etiqueta en Factory | menú Factory |

## El sistema, visto desde adentro

```
$ busybox uname -a
Linux mt5882 3.10.27 #1 SMP PREEMPT Mon Oct 12 01:24:32 CST 2015 armv7l GNU/Linux

$ busybox cat /proc/cpuinfo | grep -E "Hardware|model name"
model name  : ARMv7 Processor rev 3 (v7l)
Features    : swp half thumb fastmult vfp edsp neon vfpv3 tls vfpv4 idiva idivt
Hardware    : MT5882

$ /lib/libc.so.6 | head -1
GNU C Library (GNU libc) stable release version 2.18
```

Cuatro núcleos Cortex-A7 con NEON y VFPv4, **454 MB** de RAM, glibc 2.18 y ABI **soft-float**
(`e_flags 0x05000200`) — dato clave para compilar cualquier cosa (ver [doc 06](06-cadena-de-herramientas.md)).

## Dos navegadores, no uno

Esto es importante y se descubre mirando los procesos y los scripts de arranque:

| Motor | Binario | Qué corre | Capacidades |
|---|---|---|---|
| **Opera Presto 2.12** | `/3rd/browser/lib/libopera.so.3.6` | menú principal (`hisenseUI`), launcher, TV Store | ES5, sin `fetch`/`Promise`/WASM |
| **Opera OMI 4.2 (Blink)** | `/3rd/browser_engine/libomi.so` | YouTube, Netflix, HbbTV | `Promise`, MSE, WebGL, **DevTools en 9222** |

El segundo se identifica en `/json/version` de DevTools como
`Chrome/35.0.1916.153 … OPR/22.0.1481.0 OMI/4.2.12.48`. No es un Chrome de escritorio: es el
motor OMI de Opera con backend DirectFB.

`/3rd/internet_browser/browser` (170 KB) es solo el lanzador; el motor real es `libomi.so` (40 MB).

## Menú de fábrica

Se entra por: **Sonido → Configuración avanzada de audio → Balance en 0 → secuencia `1 9 6 9`**.

Muestra versión, claves (`HDCP2.2 key: OK`, `WIDEVINE key: NG`), balance de blancos, `CHANNEL
INIT`, `Clean Chip`, `Write Keys`, tests de patrón. **No** hay opción de consola, ni de SSH, ni de
exportar firmware. El manual de servicio de otra variante menciona `URAT on/off` (UART), pero esa
opción **no existe** en esta unidad.

> `WIDEVINE key: NG` no es un error de lectura: los directorios del keybox están vacíos de verdad
> (`/3rd_rw/widevine/keybox` y `/3rd_rw/widevine_eme/keybox`). Sin claves de dispositivo no hay
> Netflix ni Prime Video posibles.

## Cómo confirmar el modelo en otra unidad

1. Menú de usuario → Información → versión de software.
2. Menú Factory (secuencia 1969) → versión y claves.
3. `GET http://<ip>:56790/dd.xml` → fabricante y modelo.
4. SSDP (`M-SEARCH` a 239.255.255.250:1900) → `SERVER: Linux/3.10.27, UPnP/1.0, Portable SDK
   for UPnP devices/1.6.13`.
5. Navegador de apps → `http://localhost:9222/json/version` → UA completo con `OMI/...`.
