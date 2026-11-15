#!/usr/bin/env python3
"""Descarga binaria desde el TV por SSH, sin corromper el contenido.

`ssh host 'cat archivo' > local` desde PowerShell rompe los binarios (convierte a texto).
Esto abre el archivo local en modo 'wb' y le pasa el handle al stdout de ssh.

Uso:
    python descargar.py /data/respaldos/iqqi.tar.gz  ../respaldos/iqqi.tar.gz
    python descargar.py /data/respaldos/iqqi.manifest ../respaldos/iqqi.manifest
"""
import os
import subprocess
import sys

TV = "root@192.168.0.56"
PUERTO = "2222"
LLAVE = os.path.expanduser(r"~\.ssh\tv_hisense")


def descargar(remoto: str, local: str) -> None:
    os.makedirs(os.path.dirname(os.path.abspath(local)), exist_ok=True)
    cmd = ["ssh", "-i", LLAVE, "-p", PUERTO, "-o", "BatchMode=yes",
           TV, f"busybox cat {remoto}"]
    with open(local, "wb") as f:
        r = subprocess.run(cmd, stdout=f, stderr=subprocess.PIPE)
    if r.returncode != 0:
        sys.exit(f"fallo ssh: {r.stderr.decode('utf-8', 'replace')}")
    print(f"{remoto} -> {local}  ({os.path.getsize(local)} bytes)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    descargar(sys.argv[1], sys.argv[2])
