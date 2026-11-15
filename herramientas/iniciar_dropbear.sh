#!/bin/sh
# Se ejecuta EN el TV (queda en /data/bin). Nada de esto sobrevive a un reinicio: se repite a mano
# (o desde el icono "HS Update" del launcher, que abre la shell 2323 por la que se corre este script).
# 1) /dev/pts: sin esto dropbear acepta la conexión pero no puede abrir la terminal (PTY allocation
#    request failed / shell request failed al conectar con ssh).
# 2) /etc/passwd es de solo lectura: se superpone una copia con el home de root en /data/root.
# 3) arranca dropbear en el 2222 (solo clave publica, sin contraseña).
busybox mount | busybox grep -q ' /dev/pts ' || { busybox mkdir -p /dev/pts; busybox mount -t devpts devpts /dev/pts; }
busybox mount | busybox grep -q ' /etc/passwd ' || { busybox sed 's#:/basic:#:/data/root:#' /etc/passwd > /data/passwd && busybox mount --bind /data/passwd /etc/passwd; }
busybox ps | busybox grep -q '[d]ropbear -p 2222' || /data/bin/dropbear -p 2222 -r /data/dropbear/host_ed25519
echo "dropbear en :2222"

# Levanta solo HTTP para el panel/control cuando el hook inicia SSH.
if [ -x /data/bin/service.sh ]; then
    /bin/busybox mkdir -p /data/log
    /bin/sh /data/bin/service.sh start_http >>/data/log/control-boot.log 2>&1
fi
