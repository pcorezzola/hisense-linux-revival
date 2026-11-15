#!/bin/sh
# install.sh - payload del hook de arranque /3rd_rw/upgrade/ModernTV_Launcher.pkg
#
# Lo ejecuta run-Launcher (via /3rd_rw/software-update/appselfupgrade.sh) en CADA arranque
# del TV, como root, ANTES de mostrar la interfaz. Es el mecanismo de auto-actualizacion del
# launcher que el propio fabricante dejo armado; no requiere tocar el rootfs ni flashear nada.
#
# Reglas de oro para no romper el arranque:
#   - No bloquear: todo lo que tarde va en background.
#   - No fallar nunca: siempre exit 0.
#   - No escribir fuera de /data y /3rd_rw.

LOG=/data/hook.log

# PATH fijo: run-Launcher hereda un entorno minimo y sin esto 'busybox' a secas
# puede no resolverse (se comprobo en la replica: 'id: command not found').
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/data/bin
export PATH

say() { echo "[hook $(/bin/busybox date '+%Y-%m-%d %H:%M:%S')] $*" >> "$LOG"; }

say "arranque: install.sh ejecutado uid=$(/bin/busybox id -u)"

# 1) Servicios: dropbear con clave publica (idempotente; no hace nada si ya corre).
if [ -x /data/bin/iniciar_dropbear.sh ]; then
    /bin/sh /data/bin/iniciar_dropbear.sh >> "$LOG" 2>&1
    say "iniciar_dropbear.sh -> $?"
else
    say "sin /data/bin/iniciar_dropbear.sh (nada que arrancar)"
fi

# 1b) Panel web/servicios (httpd 8080, telnetd 2324). Desactivado a proposito:
#     descomentar solo si se quiere que el panel arranque solo en cada encendido.
# if [ -x /data/bin/boot.sh ]; then /bin/sh /data/bin/boot.sh; say "boot.sh (panel) -> $?"; fi

# 2) Cache del navegador de apps: es el 60% de /3rd_rw (llego a 417 MB de 726 MB).
#    El propio firmware la borra con clear_browser_cache() en update.sh. Se limpia solo
#    cuando pasa el umbral, para no castigar el arranque borrando 400 MB por gusto.
CACHE_BASE=/3rd_rw/internet_browser
CACHE_LIMITE_MB=150
if [ -d "$CACHE_BASE" ]; then
    TAM_MB=$(/bin/busybox du -sm "$CACHE_BASE" 2>/dev/null | /bin/busybox awk '{print $1}')
    if [ -z "$TAM_MB" ]; then
        say "cache: no se pudo medir $CACHE_BASE"
    elif [ "$TAM_MB" -gt "$CACHE_LIMITE_MB" ]; then
        if /bin/busybox ps | /bin/busybox grep -q '[i]nternet_browser/browser'; then
            say "cache ${TAM_MB} MB pero el navegador de apps esta abierto: no se toca"
        else
            /bin/busybox rm -rf "$CACHE_BASE"/*/Cache/*
            NUEVO_MB=$(/bin/busybox du -sm "$CACHE_BASE" 2>/dev/null | /bin/busybox awk '{print $1}')
            say "cache limpiada: ${TAM_MB} MB -> ${NUEVO_MB} MB"
        fi
    else
        say "cache ${TAM_MB} MB (limite ${CACHE_LIMITE_MB} MB): sin cambios"
    fi
fi

# 2b) Foto de espacio libre, util para detectar que se vuelve a llenar.
say "espacio: $(/bin/busybox df -h /3rd_rw 2>/dev/null | /bin/busybox tail -1)"

# 3) Re-armar el hook para el proximo arranque.
#    appselfupgrade.sh hace 'rm -rf' del .pkg justo despues de volver de este script,
#    por eso la copia se hace desde un proceso en segundo plano con una espera corta.
if [ -f /data/hook/ModernTV_Launcher.pkg ]; then
    (
        /bin/busybox sleep 5
        /bin/busybox mkdir -p /3rd_rw/upgrade
        /bin/busybox cp -f /data/hook/ModernTV_Launcher.pkg /3rd_rw/upgrade/ModernTV_Launcher.pkg
        say "hook re-armado para el proximo arranque: $(/bin/busybox ls -l /3rd_rw/upgrade/ModernTV_Launcher.pkg 2>&1)"
    ) &
fi

say "fin"
exit 0
