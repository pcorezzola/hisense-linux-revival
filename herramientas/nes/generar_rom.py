#!/usr/bin/env python3
"""Genera un ROM NES mínimo (iNES) para validar el emulador sin usar ROMs de terceros.

El programa 6502: espera dos vblanks, carga la paleta, llena el nametable con el tile 1
(color sólido), limpia la tabla de atributos, hace scroll 0 y enciende el fondo.
No habilita NMI (el vector apunta a un RTI) para que no se reinicie cada frame.

Salida: www/roms/prueba.nes
"""
import os
import struct

AQUI = os.path.dirname(os.path.abspath(__file__))
SALIDA = os.path.join(AQUI, "..", "www", "roms", "prueba.nes")

PRG_OFF = 0x4000          # el código vive en $C000 dentro de 32 KB de PRG
BASE = 0xC000


def ensamblar():
    """Devuelve la lista de bytes del programa, empezando en $C000."""
    c = bytearray()

    def emitir(*b):
        c.extend(b)

    # reset
    emitir(0x78)                     # SEI
    emitir(0xD8)                     # CLD
    emitir(0xA2, 0xFF)               # LDX #$FF
    emitir(0x9A)                     # TXS
    emitir(0x2C, 0x02, 0x20)         # BIT $2002
    p1 = len(c)
    emitir(0x2C, 0x02, 0x20)         # espera vblank 1
    emitir(0x10, 0xFB)               # BPL p1
    p2 = len(c)
    emitir(0x2C, 0x02, 0x20)         # espera vblank 2
    emitir(0x10, 0xFB)               # BPL p2

    # paleta: $3F00
    emitir(0xA9, 0x3F); emitir(0x8D, 0x06, 0x20)
    emitir(0xA9, 0x00); emitir(0x8D, 0x06, 0x20)
    emitir(0xA2, 0x00)               # LDX #$00
    pal_dir = PRG_OFF + 0xA0          # la paleta se guarda al final
    emitir(0xBD, pal_dir & 0xFF, pal_dir >> 8)   # LDA paleta,X
    emitir(0x8D, 0x07, 0x20)         # STA $2007
    emitir(0xE8)                     # INX
    emitir(0xE0, 0x20)               # CPX #$20
    emitir(0xD0, 0xF5)               # BNE

    # nametable $2000: 4 páginas de tile 1
    emitir(0xA9, 0x20); emitir(0x8D, 0x06, 0x20)
    emitir(0xA9, 0x00); emitir(0x8D, 0x06, 0x20)
    emitir(0xA2, 0x00)               # LDX #$00
    emitir(0xA0, 0x04)               # LDY #$04
    relleno = len(c)
    emitir(0xA9, 0x01)               # LDA #$01  (tile 1)
    emitir(0x8D, 0x07, 0x20)         # STA $2007
    emitir(0xCA)                     # DEX
    emitir(0xD0, 0xF8)               # BNE relleno
    emitir(0x88)                     # DEY
    emitir(0xD0, 0xF5)               # BNE relleno

    # tabla de atributos $23C0: 64 bytes en 0
    emitir(0xA9, 0x23); emitir(0x8D, 0x06, 0x20)
    emitir(0xA9, 0xC0); emitir(0x8D, 0x06, 0x20)
    emitir(0xA2, 0x40)               # LDX #$40
    atrib = len(c)
    emitir(0xA9, 0x00)               # LDA #$00
    emitir(0x8D, 0x07, 0x20)         # STA $2007
    emitir(0xCA)                     # DEX
    emitir(0xD0, 0xF8)               # BNE atrib

    # scroll 0
    emitir(0xA9, 0x00); emitir(0x8D, 0x05, 0x20)
    emitir(0x8D, 0x05, 0x20)

    # PPUCTRL sin NMI (0x00) y PPUMASK con fondo encendido (0x0A)
    emitir(0xA9, 0x00); emitir(0x8D, 0x00, 0x20)
    emitir(0xA9, 0x0A); emitir(0x8D, 0x01, 0x20)

    # bucle infinito
    bucle = BASE + len(c)
    emitir(0x4C, bucle & 0xFF, bucle >> 8)

    # RTI para el NMI (no se usa, pero por las dudas)
    rti = BASE + len(c)
    emitir(0x40)

    # rellenar hasta la paleta (offset 0xA0) con 0xEA (NOP)
    while len(c) < 0xA0:
        emitir(0xEA)

    # paleta de 32 bytes
    c.extend(bytes([
        0x0F, 0x01, 0x11, 0x21,
        0x0F, 0x06, 0x16, 0x26,
        0x0F, 0x09, 0x19, 0x29,
        0x0F, 0x02, 0x12, 0x22,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
    ]))

    return bytes(c), rti


def main():
    codigo, rti = ensamblar()

    # PRG de 32 KB (2 bancos), código en el segundo
    prg = bytearray(b"\xEA" * 0x8000)
    prg[PRG_OFF:PRG_OFF + len(codigo)] = codigo

    # vectores en $FFFA-$FFFF (offset 0x7FFA del PRG): NMI, RESET, IRQ
    struct.pack_into("<HHH", prg, 0x7FFA, rti, BASE, rti)

    # CHR de 8 KB: tile 1 = bloque sólido (plano 0 en 1, plano 1 en 0)
    chr = bytearray(0x2000)
    chr[16:24] = b"\xFF" * 8          # plano 0 del tile 1
    chr[24:32] = b"\x00" * 8          # plano 1 del tile 1

    cabecera = b"NES\x1a" + bytes([2, 1, 0x00, 0x00]) + bytes(8)
    rom = cabecera + bytes(prg) + bytes(chr)

    os.makedirs(os.path.dirname(SALIDA), exist_ok=True)
    with open(SALIDA, "wb") as f:
        f.write(rom)
    print(f"escrito {SALIDA} ({len(rom)} bytes)")
    print(f"  código en $C000, {len(codigo)} bytes; RTI en ${rti:04X}")


if __name__ == "__main__":
    main()
