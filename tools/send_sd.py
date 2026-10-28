#!/usr/bin/env python3
"""Envia los sprites empaquetados a la SD de la placa por USB.

  python3 tools/send_sd.py                  # envia tools/sdcard/mons/*.bin
  python3 tools/send_sd.py --port /dev/cu.usbmodem101
  python3 tools/send_sd.py --ls             # lista lo que hay en la SD
  python3 tools/send_sd.py --no-shiny       # sin pNNN shiny (1.75C: no caben en la flash)
"""
import argparse
import glob
import os
import sys
import time
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
    ap.add_argument('--no-shiny', action='store_true', help='omite los ps*.bin')
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

    for path in files:
        size = os.path.getsize(path)
        name = f"mons/{os.path.basename(path)}"
        print(f"-> {name} ({size/1024:.0f} KB)")
        ser.write(f"PUT {name} {size}\n".encode())
        if not wait_line(ser, 'OK', 5):
            print("   la placa no acepto el PUT, sigo con el siguiente")
            continue
        t0 = time.time()
        ok = True
        with open(path, 'rb') as f:
            while chunk := f.read(2048):
                ser.write(chunk)
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
