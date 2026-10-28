#!/usr/bin/env python3
"""Envia los sprites empaquetados a la SD de la placa por USB.

  python3 tools/send_sd.py                  # envia tools/sdcard/mons/*.bin
  python3 tools/send_sd.py --port /dev/cu.usbmodem101
  python3 tools/send_sd.py --ls             # lista lo que hay en la SD
  python3 tools/send_sd.py --only thumbs    # send just the files matching

--only exists because thumbs.bin changes every time the dex grows, and it is
one 415 KB file against ~105 MB of sprites. Without it the only way to fix a
stale gallery was to resend everything.
  python3 tools/send_sd.py --no-shiny       # sin los psNNN shiny
  python3 tools/send_sd.py --raw            # sin comprimir (firmware sin TPKZ)

Los TPK2 se mandan comprimidos con zlib (TPKZ, ~34x menos): asi caben normales
y shinies en la FAT interna de la 1.75C y la subida tarda segundos.
"""
import argparse
import glob
import os
import struct
import sys
import time
import zlib
import serial

def find_port():
    ports = glob.glob('/dev/cu.usbmodem*')
    if not ports:  # Windows/Linux: el USB nativo del ESP32-S3 (VID 0x303A)
        from serial.tools import list_ports
        ports = [p.device for p in list_ports.comports() if p.vid == 0x303A]
    if not ports:
        sys.exit("no encuentro la placa (/dev/cu.usbmodem* o VID 303A)")
    return ports[0]

def wait_line(ser, expect, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        line = ser.readline().decode(errors='replace').strip()
        if not line:
            continue
        print(f"  placa: {line}")
        if line == expect:
            return True
        if line == 'ERR':
            return False
    return False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port')
    ap.add_argument('--ls', action='store_true')
    ap.add_argument('--only', help='only send files whose name contains this')
    ap.add_argument('--no-shiny', action='store_true', help='omite los ps*.bin')
    ap.add_argument('--raw', action='store_true',
                    help='envia los TPK2 sin comprimir (firmware sin soporte TPKZ)')
    args = ap.parse_args()

    port = args.port or find_port()
    print(f"puerto {port}")
    ser = serial.Serial(port, 115200, timeout=1)
    time.sleep(1.5)
    ser.reset_input_buffer()

    if args.ls:
        ser.write(b'LS\n')
        wait_line(ser, 'DONE', 5)
        return

    files = sorted(glob.glob(os.path.join(os.path.dirname(__file__), 'sdcard', 'mons', '*.bin')))
    if args.no_shiny:
        files = [f for f in files if not os.path.basename(f).startswith('ps')]
    if not files:
        sys.exit("no hay .bin; ejecuta antes tools/pack_pmd.py")
    if args.only:
        files = [f for f in files if args.only in os.path.basename(f)]
        if not files:
            sys.exit(f"nothing matches --only {args.only}")
        print(f"--only {args.only}: {len(files)} file(s)")

    for path in files:
        with open(path, 'rb') as f:
            data = f.read()
        if not args.raw and data[:4] == b'TPK2':  # TPKZ: el firmware lo descomprime al cargar
            data = b'TPKZ' + struct.pack('<I', len(data)) + zlib.compress(data, 9)
        size = len(data)
        name = f"mons/{os.path.basename(path)}"
        print(f"-> {name} ({size/1024:.0f} KB)")
        ser.write(f"PUT {name} {size}\n".encode())
        if not wait_line(ser, 'OK', 5):
            print("   la placa no acepto el PUT, sigo con el siguiente")
            continue
        t0 = time.time()
        ok = True
        for i in range(0, size, 2048):
            ser.write(data[i:i + 2048])
            # espera el ack '#' del bloque
            ack = ''
            while ack not in ('#', 'ERR'):
                ack = ser.readline().decode(errors='replace').strip()
                if ack == '':
                    ok = False
                    break
            if not ok or ack == 'ERR':
                ok = False
                break
        if ok and wait_line(ser, 'DONE', 30):
            kbs = size / 1024 / max(0.01, time.time() - t0)
            print(f"   ok ({kbs:.0f} KB/s)")
        else:
            print("   fallo la transferencia")
    print("listo")

if __name__ == '__main__':
    main()
