# 09 — Recetas (copiar y pegar)

Ajustar `TV=192.168.0.56` y la clave `~/.ssh/tv_hisense` a tu caso.

## Conexión

```sh
TV=192.168.0.56
ssh -i ~/.ssh/tv_hisense -p 2222 root@$TV
```

Si el televisor se reinició y no responde, mirar primero `/data/hook.log`: el hook de arranque
tiene que haberse ejecutado.

## Estado general

```sh
ssh ... 'busybox uptime; busybox free; busybox df -h'
ssh ... 'busybox cat /tmp/am_status'                      # app en primer plano
ssh ... 'busybox ps | busybox grep -E "launcher|browser|app_man"'
ssh ... 'busybox tail -5 /data/hook.log'
ssh ... 'busybox netstat -ltn'                            # puertos a la escucha
```

## Lanzar / cerrar apps (DIAL)

```sh
curl -s -X POST -d '' -H 'Content-Type: text/plain' http://$TV:56789/apps/YouTube
curl -s -X DELETE http://$TV:56789/apps/YouTube/run
curl -s http://$TV:56789/apps/YouTube        # estado (poco confiable)
```

## DevTools del navegador de apps (CDP)

Con una app de internet corriendo, el puerto 9222 está abierto:

```python
import json, urllib.request, websocket
p = [x for x in json.loads(urllib.request.urlopen("http://192.168.0.56:9222/json").read())
     if x["type"] == "page"][0]
ws = websocket.create_connection(p["webSocketDebuggerUrl"], timeout=20, suppress_origin=True)

def cdp(i, metodo, params=None):
    ws.send(json.dumps({"id": i, "method": metodo, "params": params or {}}))
    while True:
        r = json.loads(ws.recv())
        if r.get("id") == i:
            return r

cdp(1, "Page.navigate", {"url": "http://127.0.0.1:8092/"})
print(cdp(2, "Runtime.evaluate", {"expression": "document.title", "returnByValue": True}))
```

Las teclas funcionan con `Input.dispatchKeyEvent` (flechas: 38/40/37/39, Enter: 13). **El mouse
no**: el perfil de la app `youtube` tiene `[MOUSE_MODE] enablemouse=0`.

## Servir una página desde el propio televisor

```sh
ssh ... 'busybox mkdir -p /data/www_mia && /bin/busybox httpd -f -p 8092 -h /data/www_mia &'
# luego, en el navegador:  http://127.0.0.1:8092/
```

Útil porque el mismo origen evita CORS y no depende de ninguna PC.

## Agregar una app al launcher

```sh
busybox cp /3rd_rw/appdata/launcher/Appinfo.json /3rd_rw/appdata/launcher/Appinfo.json.bak
```

Después escribir un JSON como el del [doc 03](03-conseguir-root.md#2-colgarse-de-la-lista-de-apps-del-launcher)
y reiniciar el launcher (o el televisor).

## Limpiar la caché del navegador (recupera cientos de MB)

```sh
ssh ... 'busybox ps | busybox grep "[i]nternet_browser" || \
         busybox rm -rf /3rd_rw/internet_browser/*/Cache/*; busybox df -h /3rd_rw'
```

## Respaldo de particiones

Por USB (rápido, ~2 minutos las 26 particiones):

```sh
busybox dd if=/dev/mmcblk0p7 of=/dev/shm/shm_tmp/mnt/usb/sda1/p7.img bs=1M
busybox sha256sum /dev/mmcblk0p7 /dev/shm/shm_tmp/mnt/usb/sda1/p7.img
```

Por red, sirviendo la partición con `nc` y descargándola con Python (el televisor solo lee):

```sh
# en el TV
busybox nc -l -p 9000 < /dev/mmcblk0p7
# en la PC
python -c "import socket,shutil;s=socket.create_connection(('192.168.0.56',9000));f=open('p7.img','wb');shutil.copyfileobj(s.makefile('rb'),f)"
```

Los nodos de `boot0`/`boot1` no existen en `/dev`: crearlos con
`busybox mknod /dev/shm/boot0 b 179 64` (y `b 179 128` para boot1).

## Compilar un binario chico para el TV

```sh
arm-linux-gnueabi-gcc -nostdlib -static -Os -ffreestanding \
    -march=armv7-a -mfloat-abi=soft -fno-stack-protector -fno-builtin \
    -Wl,-e,main -o hola hola.c
arm-linux-gnueabi-readelf -h hola | grep Flags     # 0x5000200 = soft-float, correcto
```

## Subir un binario

```sh
ssh -i ~/.ssh/tv_hisense -p 2222 root@$TV 'busybox cat > /data/bin/x && busybox chmod +x /data/bin/x' < x
```

## Reiniciar el televisor

```sh
ssh ... 'busybox sync; busybox reboot -f'
```

Si el comando queda colgado, es normal: la conexión se corta. El SSH vuelve solo (lo levanta el
hook de arranque).
