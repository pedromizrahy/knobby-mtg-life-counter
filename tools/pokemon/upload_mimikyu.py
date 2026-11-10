#!/usr/bin/env python3
"""Safely transfer a user-generated TamaPoke Mimikyu TPK2 to the DIAL microSD.

Usage: py -3 tools/pokemon/upload_mimikyu.py --port COM4 --file C:\\dev\\TamaPoke\\tools\\sdcard\\mons\\p778.bin

Requires: py -3 -m pip install pyserial
Close Arduino Serial Monitor before running. DO NOT modify existing p778.bin.
"""
import argparse
from pathlib import Path
import struct
import time
import zlib

import serial


def await_line(port, expected, timeout=12):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = port.readline().decode("utf-8", errors="replace").strip()
        if value:
            print("DIAL:", value)
        if value == expected:
            return
        if "ERR" in value or "SKIPPED" in value:
            raise RuntimeError(f"DIAL refused transfer: {value}")
    raise TimeoutError(f"No {expected!r} from DIAL")


def inspect(path):
    blob = path.read_bytes()
    if not (7 <= len(blob) <= 3 * 1024 * 1024):
        raise ValueError("Invalid file size (expected 7 bytes to 3 MiB)")
    if blob[:4] != b"TPK2":
        raise ValueError("This is not a TamaPoke TPK2 container")
    acts, palette = struct.unpack_from("<BH", blob, 4)
    if not 1 <= acts <= 12 or not 1 <= palette <= 256:
        raise ValueError("Invalid TPK2 header")
    offset = 7 + palette * 2
    used = set()
    for _ in range(acts):
        if offset + 4 > len(blob):
            raise ValueError("Truncated TPK2 action")
        aid, w, h, nf = struct.unpack_from("<4B", blob, offset)
        offset += 4
        if aid in used or aid >= 12 or w == 0 or h == 0 or not 1 <= nf <= 24:
            raise ValueError("Invalid TPK2 action")
        used.add(aid)
        offset += nf * 2 + w * h * nf
        if offset > len(blob):
            raise ValueError("Truncated TPK2 pixels")
    if offset != len(blob) or 0 not in used:
        raise ValueError("Invalid TPK2 end or missing Idle animation")
    return blob


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--file", required=True, type=Path)
    args = ap.parse_args()
    blob = inspect(args.file)
    crc = zlib.crc32(blob)
    print(f"Mimikyu asset: {len(blob)} bytes, CRC32 {crc:08x}")
    print("Transfer will only create /mons/p778.bin if it does not exist.")
    with serial.Serial(args.port, 115200, timeout=1, write_timeout=10) as device:
        time.sleep(1.5)
        device.reset_input_buffer()
        cmd = f"PK PMD PUT 778 {len(blob)} {crc:08x}\n"
        device.write(cmd.encode("ascii"))
        device.flush()
        await_line(device, "PMD_READY")
        time.sleep(0.2)  # Let USB CDC finish the text-to-binary transition.
        for i in range(0, len(blob), 256):
            device.write(blob[i:i + 256])
            device.flush()
            await_line(device, "#")
        end = device.readline().decode("utf-8", errors="replace").strip()
        print("DIAL:", end)
        if "[Pokemon] PMD PUT DONE" not in end:
            raise RuntimeError("Transfer did not finish successfully; do not assume asset installed")
        print("Transfer complete. Reopen serial monitor and run PK PMD CHECK.")


if __name__ == "__main__":
    main()
