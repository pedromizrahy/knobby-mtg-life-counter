#!/usr/bin/env python3
"""Audit first; optionally upload TPK2 files by National Dex number.

DRY RUN by default. This tool does not delete or replace files on DIAL.
Requires new generic firmware handler before --execute.
Existing Mimikyu #778 is skipped by default.
"""
import argparse
import json
import re
import time
import zlib
from pathlib import Path
from audit_catalog import validate_tpk2

NAME = re.compile(r"^p([1-9][0-9]{0,3})\.bin$")


def plan(catalog_path, mons_dir, include_mimikyu=False):
    manifest = json.loads(catalog_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != 1 or not isinstance(manifest.get("species"), list):
        raise ValueError("Invalid catalog schema")
    files = []
    ids = set()
    for row in manifest["species"]:
        number = row["id"]
        asset = row["asset"]
        if type(number) is not int or not 1 <= number <= 1025 or number in ids:
            raise ValueError("Invalid or duplicate species")
        ids.add(number)
        match = NAME.fullmatch(asset)
        if not match or int(match.group(1)) != number:
            raise ValueError("Unexpected sprite filename")
        if number == 778 and not include_mimikyu:
            continue
        path = mons_dir / asset
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"Missing/unsafe sprite: {path}")
        validate_tpk2(path)
        data = path.read_bytes()
        files.append((number, data, zlib.crc32(data)))
    return files


def receive(port, expected, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        text = port.readline().decode("utf-8", errors="replace").strip()
        if text:
            print("DIAL:", text)
        if text == expected:
            return
        if "ERR" in text or "SKIPPED" in text:
            raise RuntimeError(f"Device rejected operation: {text}")
    raise TimeoutError(f"Expected {expected} not received")


def verify_installed(port, species, timeout=30):
    """Read-only on-device structural verification after upload."""
    port.write(f"PK PMD CHECK {species}\n".encode("ascii"))
    port.flush()
    deadline = time.monotonic() + timeout
    valid = False
    while time.monotonic() < deadline:
        response = port.readline().decode("utf-8", errors="replace").strip()
        if response:
            print("DIAL:", response)
        if "PMD INVALID" in response or "PMD FAIL" in response or "PMD MISSING" in response or "SKIPPED" in response:
            raise RuntimeError(f"Installed species #{species} failed verification: {response}")
        if "[Pokemon] PMD VALID " in response:
            valid = True
        if "PMD CHECK COMPLETE" in response:
            if valid:
                return
            raise RuntimeError(f"Verification for #{species} completed without valid TPK2")
    raise TimeoutError(f"Did not receive completion of PK PMD CHECK {species}")


def upload(port, species, data, crc):
    port.reset_input_buffer()
    port.write(f"PK PMD PUT {species} {len(data)} {crc:08x}\n".encode("ascii"))
    port.flush()
    receive(port, "PMD_READY")
    time.sleep(0.2)
    for offset in range(0, len(data), 256):
        port.write(data[offset:offset + 256])
        port.flush()
        receive(port, "#", timeout=25)
    receive_done = f"[Pokemon] PMD PUT DONE size={len(data)} crc32={crc:08x}"
    receive(port, receive_done, timeout=25)
    verify_installed(port, species)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--catalog", type=Path,
                    default=Path(__file__).with_name("candidate_pool.json"))
    ap.add_argument("--mons-dir", type=Path, required=True)
    ap.add_argument("--port", default="COM4")
    ap.add_argument("--execute", action="store_true",
                    help="Explicitly transfer assets; default is a dry run")
    ap.add_argument("--include-mimikyu", action="store_true",
                    help="Include #778 (existing asset is still never overwritten)")
    args = ap.parse_args()
    batch = plan(args.catalog, args.mons_dir, args.include_mimikyu)
    for species, data, crc in batch:
        print(f"#{species:04d} {len(data):>8} bytes CRC32={crc:08x}")
    print(f"{len(batch)} valid candidate sprites; mode={'TRANSFER' if args.execute else 'DRY RUN'}")
    if not args.execute:
        print("No writes performed. New generic firmware must be installed before --execute.")
        return
    try:
        import serial
    except ImportError as e:
        raise SystemExit("Install pyserial: py -3 -m pip install pyserial") from e
    with serial.Serial(args.port, 115200, timeout=1, write_timeout=10) as port:
        time.sleep(1.5)
        for species, data, crc in batch:
            print(f"Uploading #{species}...")
            upload(port, species, data, crc)
    print("All uploads confirmed by device. Existing sprites were never overwritten.")


if __name__ == "__main__":
    main()
