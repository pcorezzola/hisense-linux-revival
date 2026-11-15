#!/usr/bin/env python3
"""Vuelca por red la eMMC del TV Hisense (solo lectura) a la PC, verificando hashes.

Cómo funciona (nada se instala ni se escribe en el TV):
  1. Por la shell root (puerto 2323) se lanza  `busybox nc -l -p PUERTO < /dev/mmcblk0pN`
     -> el TV sirve la partición por TCP; la PC se conecta y la descarga (la PC sale, el TV no
     necesita regla de firewall entrante en Windows).
  2. La PC calcula SHA-256 mientras recibe y el TV calcula el suyo con `busybox sha256sum`;
     se comparan.  (p10 y p12 están montadas en lectura-escritura: un hash distinto ahí
     solo significa que el TV escribió algo durante la copia, se marca como aviso.)
  3. /data (2 GB, casi vacía) no se vuelca cruda: se hace `tar` de su contenido.
  4. mmcblk0rpmb se omite (zona protegida, no es legible en crudo).

Uso:
  python volcar_tv.py                      # todo
  python volcar_tv.py --lista              # solo muestra el plan
  python volcar_tv.py --solo rootfsA perm  # solo esas particiones (nombres del mtdparts)
"""
import argparse
import hashlib
import json
import re
import socket
import sys
import time
from datetime import datetime
from pathlib import Path

from tv_exec import run as tv_run

# Nombres en el orden en que aparecen en el mtdparts del kernel (mmcblk0p1..p27)
NOMBRES = ["uboot", "uboot_env", "part_02", "part_03", "kernelA", "kernelB", "rootfsA", "rootfsB",
           "basic", "perm", "3rd_ro", "rw_area", "reserved", "channelA", "channelB", "pq", "aq",
           "logo", "acfg", "sched_pvr", "adsp", "resv21", "resv22", "facset", "resv24", "hiscfg", "data"]
MONTADAS_RW = {"perm", "rw_area", "data"}
PUERTO_BASE = 9100


def tv(cmd, a, timeout=30):
    return tv_run(cmd, a.host, 2323, timeout)[0]


def leer_particiones(a):
    out = tv("cat /proc/partitions", a)
    sizes = {}
    for m in re.finditer(r"^\s*179\s+\d+\s+(\d+)\s+(mmcblk0\S*)\s*$", out, re.M):
        sizes[m.group(2)] = int(m.group(1)) * 1024
    plan = []
    for i, nombre in enumerate(NOMBRES, 1):
        dev = f"mmcblk0p{i}"
        if dev in sizes:
            plan.append((f"p{i:02d}_{nombre}", nombre, f"/dev/{dev}", sizes[dev]))
    for dev, nombre in (("mmcblk0boot0", "boot0"), ("mmcblk0boot1", "boot1")):
        if dev in sizes:
            plan.append((nombre, nombre, f"/dev/{dev}", sizes[dev]))
    return plan


def recibir(host, puerto, destino: Path, esperado, a, dev=None):
    """Descarga hasta `esperado` bytes. Si el Wi-Fi se corta, reanuda desde el último bloque de 64 KB
    pidiendo al TV un `dd skip=`. Devuelve (bytes, sha256 calculado sobre el archivo final)."""
    BLQ = 65536
    reintentos = 0
    destino.write_bytes(b"")
    n = 0
    t0 = ult = time.time()
    while True:
        puerto_i = puerto + reintentos * 100
        if reintentos:
            tv(f"(busybox dd if={dev} bs={BLQ} skip={n // BLQ} 2>/dev/null | busybox nc -l -p {puerto_i} >/dev/null 2>&1 &) ; sleep 1", a, 15)
        s = None
        for _ in range(15):
            try:
                s = socket.create_connection((host, puerto_i), timeout=5)
                break
            except OSError:
                time.sleep(0.7)
        if s is None:
            raise RuntimeError(f"no pude conectar a {host}:{puerto_i}")
        s.settimeout(30 if esperado else 5)
        cortado = False
        with destino.open("ab") as f:
            while esperado is None or n < esperado:
                try:
                    d = s.recv(1 << 20)
                except (socket.timeout, ConnectionError):
                    if esperado is None:
                        break
                    cortado = True
                    break
                if not d:
                    break
                if esperado is not None and n + len(d) > esperado:
                    d = d[: esperado - n]
                f.write(d)
                n += len(d)
                if time.time() - ult > 3:
                    ult = time.time()
                    mb = n / 1048576
                    print(f"\r    {mb:8.1f} MB  {mb / (ult - t0):5.1f} MB/s", end="", flush=True)
        s.close()
        if esperado is None or n >= esperado:
            break
        if not dev or reintentos >= 20:
            raise RuntimeError(f"transferencia incompleta: {n}/{esperado}")
        # recortar al último bloque completo y reanudar
        n = (n // BLQ) * BLQ
        with destino.open("r+b") as f:
            f.truncate(n)
        reintentos += 1
        print(f"\r    corte a {n / 1048576:.1f} MB, reanudo (intento {reintentos})      ")
    print("\r" + " " * 40 + "\r", end="")
    h = hashlib.sha256()
    with destino.open("rb") as f:
        for blq in iter(lambda: f.read(1 << 20), b""):
            h.update(blq)
    return n, h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.0.56")
    ap.add_argument("--salida", default=str(Path(__file__).parent / "imagenes" / ("tv_" + datetime.now().strftime("%Y%m%d_%H%M"))))
    ap.add_argument("--solo", nargs="*")
    ap.add_argument("--lista", action="store_true")
    a = ap.parse_args()

    plan = leer_particiones(a)
    if not plan:
        sys.exit("No pude leer /proc/partitions del TV (¿shell 2323 activa?)")
    if a.solo:
        plan = [p for p in plan if p[1] in a.solo]
    total = sum(p[3] for p in plan if p[1] != "data")
    print(f"Plan: {len(plan)} piezas, ~{total / 1048576:.0f} MB crudos (+ tar de /data)")
    for etiqueta, nombre, dev, tam in plan:
        print(f"  {etiqueta:22s} {dev:22s} {tam / 1048576:8.2f} MB")
    if a.lista:
        return

    salida = Path(a.salida)
    (salida / "meta").mkdir(parents=True, exist_ok=True)
    for nom, cmd in {"cmdline": "cat /proc/cmdline", "partitions": "cat /proc/partitions",
                     "mount": "mount", "version": "cat /proc/version", "cpuinfo": "cat /proc/cpuinfo",
                     "meminfo": "head -3 /proc/meminfo"}.items():
        (salida / "meta" / f"{nom}.txt").write_text(tv(cmd, a, 15), encoding="utf-8")

    manifiesto = []
    for i, (etiqueta, nombre, dev, tam) in enumerate(plan):
        puerto = PUERTO_BASE + i
        print(f"[{i + 1}/{len(plan)}] {etiqueta}")
        t0 = time.time()
        if nombre == "data":
            tv(f"(busybox tar -cf - -C /data . | busybox nc -l -p {puerto} >/dev/null 2>&1 &) ; sleep 1", a, 10)
            n, sha = recibir(a.host, puerto, salida / "data_contenido.tar", None, a)
            manifiesto.append({"pieza": etiqueta, "archivo": "data_contenido.tar", "bytes": n, "sha256": sha,
                               "nota": "tar del contenido de /data (no crudo)"})
            continue
        tv(f"(busybox nc -l -p {puerto} < {dev} >/dev/null 2>&1 &) ; sleep 1", a, 10)
        archivo = f"{etiqueta}.img"
        n, sha = recibir(a.host, puerto, salida / archivo, tam, a, dev)
        h_tv = re.search(r"\b([0-9a-f]{64})\b\s+" + re.escape(dev), tv(f"busybox sha256sum {dev}", a, 600))
        h_tv = h_tv.group(1) if h_tv else None
        ok = (n == tam) and (h_tv == sha)
        estado = "OK" if ok else ("AVISO (montada rw, pudo cambiar)" if nombre in MONTADAS_RW and n == tam else "FALLO")
        print(f"    {estado}  {n}/{tam} bytes  {time.time() - t0:.0f}s  sha256={sha[:16]}…")
        manifiesto.append({"pieza": etiqueta, "dispositivo": dev, "archivo": archivo, "bytes": n, "esperado": tam,
                           "sha256_pc": sha, "sha256_tv": h_tv, "estado": estado})
        (salida / "manifiesto.json").write_text(json.dumps(manifiesto, indent=2), encoding="utf-8")

    (salida / "manifiesto.json").write_text(json.dumps(manifiesto, indent=2), encoding="utf-8")
    fallos = [m for m in manifiesto if m.get("estado") == "FALLO"]
    print(f"\nListo en {salida}  ({len(manifiesto) - len(fallos)} bien, {len(fallos)} con fallo)")
    sys.exit(1 if fallos else 0)


if __name__ == "__main__":
    main()
