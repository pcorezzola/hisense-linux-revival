# 04 — Persistencia en el arranque (sin flashear)

El problema clásico: conseguís una shell, pero al reiniciar se pierde. El rootfs es squashfs de
solo lectura y `/3rd` también; **no** se puede agregar un script de arranque ahí sin reflashear.

La solución estaba en el propio firmware: un hook de auto-actualización del launcher, que corre
**nuestro código como root en cada encendido**, desde una partición escribible.

## El hook que usa el fabricante

`/etc/autorun.sh` (rootfs, ro) lanza al arrancar:

```
/basic/autorun.sh  ->  /3rd/browser/run-Launcher
```

y `run-Launcher` (en `/3rd`, ro) hace, **antes** de `exec bin/launcher`:

```sh
appUpgradePath=/3rd_rw/software-update
if [ -f "/3rd_rw/upgrade/ModernTV_Launcher.pkg" ] && [ -d "${appUpgradePath}" ]; then
        sh ${appUpgradePath}/appselfupgrade.sh
fi
```

`/3rd_rw/software-update/appselfupgrade.sh`:

```sh
cd /3rd_rw/upgrade/
tar -zxf ModernTV_Launcher.pkg
cd ModernTV_Launcher
sh ./install.sh          # <---- nuestro script, como root
cd /
rm -rf /3rd_rw/upgrade/ModernTV_Launcher.pkg
rm -rf /3rd_rw/upgrade/ModernTV_Launcher
```

Es decir: **un `.pkg` (tar.gz) en `/3rd_rw/upgrade/` con un `install.sh` adentro** se ejecuta como
root en cada arranque. No hace falta tocar el rootfs ni flashear nada.

## El paquete

Formato exacto que espera `appselfupgrade.sh`:

```
ModernTV_Launcher.pkg      (tar.gz)
└── ModernTV_Launcher/
    └── install.sh         (se ejecuta con: sh ./install.sh)
```

Se arma así:

```sh
mkdir -p build/ModernTV_Launcher
cp install.sh build/ModernTV_Launcher/
chmod +x build/ModernTV_Launcher/install.sh
tar -czf ModernTV_Launcher.pkg -C build ModernTV_Launcher
```

## Re-armarse solo

`appselfupgrade.sh` **borra el `.pkg`** después de ejecutar `install.sh`. Para que el hook corra en
*el próximo* arranque, `install.sh` lo vuelve a copiar desde una copia guardada en `/data`, desde
un proceso en segundo plano con una espera corta (para no competir con el `rm -rf`):

```sh
if [ -f /data/hook/ModernTV_Launcher.pkg ]; then
    (
        /bin/busybox sleep 5
        /bin/busybox mkdir -p /3rd_rw/upgrade
        /bin/busybox cp -f /data/hook/ModernTV_Launcher.pkg /3rd_rw/upgrade/ModernTV_Launcher.pkg
    ) &
fi
```

## El `install.sh` que usamos

Copia completa en [`../herramientas/hook-install.sh`](../herramientas/hook-install.sh). Reglas que
respeta:

1. **No bloquear** el arranque (todo lo lento, en segundo plano).
2. **No fallar nunca** (`exit 0` siempre).
3. **Fijar `PATH`**: el entorno que hereda es mínimo y `busybox` a secas no resuelve.

Qué hace en cada arranque:

- levanta **dropbear** (SSH con clave pública en el 2222);
- si la caché del navegador de apps superó los **150 MB**, la limpia (era el 60% de `/3rd_rw`);
- deja el espacio libre y el resultado en `/data/hook.log`;
- se re-arma para el próximo encendido.

## Verificación

```sh
busybox cat /data/hook.log
# [hook ...] arranque: install.sh ejecutado uid=0
# [hook ...] iniciar_dropbear.sh -> 0
# [hook ...] cache 0 MB (limite 150 MB): sin cambios
# [hook ...] espacio: /dev/mmcblk0p12 726.5M 280.2M 431.3M 39% /3rd_rw
# [hook ...] hook re-armado para el proximo arranque
```

Otras pruebas de que corrió en el arranque (no a mano):

- `/data/passwd` (lo escribe `iniciar_dropbear.sh`) tiene la fecha del arranque.
- `busybox ps` muestra `dropbear` con `ppid=1` y arrancado a los pocos segundos.
- El `.pkg` reaparece solo en `/3rd_rw/upgrade/`.

## Reversión

```sh
busybox rm -f /3rd_rw/upgrade/ModernTV_Launcher.pkg
busybox rm -rf /data/hook
```

El rootfs y `/3rd` nunca se modificaron: no hay riesgo de brick por esto.

## Límites y cuidados

- **Una actualización de firmware oficial** reemplaza `/3rd_rw` y se lleva el hook.
- Si `/3rd_rw` se llena, el `cp` de re-armado falla y la persistencia se pierde **en silencio**:
  por eso el hook registra el espacio libre en cada arranque.
- No conviene cortar la energía mientras haya escrituras: `/data` y `/3rd_rw` son ext4 con journal
  (se recuperan solas, pero mejor no tentarlas).

## Alternativa que también existe (y no usamos)

El `rc.local` del rootfs trae:

```sh
if [ -e /sbin/sshd ]; then mount /dev/pts; /sbin/sshd; ... fi
```

Si en algún momento se reflashea un rootfs con `sshd` en `/sbin`, SSH arrancaría solo. No hace
falta: el hook logra lo mismo sin tocar el rootfs.
