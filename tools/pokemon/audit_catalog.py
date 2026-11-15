#!/usr/bin/env python3
"""Validate an installed TPK2 Pokemon catalog before enabling DIAL gacha.

Run on a local SD-card mirror (no serial/device access, no writes).
Only assets present AND structurally valid are considered available.
"""
import argparse
import json
import struct
from pathlib import Path

WEIGHTS = {
    "poke": {"common": 7500, "rare": 2000, "epic": 450, "legendary": 50},
    "great": {"common": 5500, "rare": 3000, "epic": 1300, "legendary": 200},
    "ultra": {"common": 3000, "rare": 4000, "epic": 2400, "legendary": 600},
    "master": {"common": 0, "rare": 0, "epic": 0, "legendary": 10000},
}
TIERS = tuple(WEIGHTS["poke"])


def validate_tpk2(path):
    """Parse the complete container, check bounds and required idle action."""
    data = path.read_bytes()
    if not 7 <= len(data) <= 3 * 1024 * 1024:
        raise ValueError("TPK2 size outside supported range")
    if data[:4] != b"TPK2":
        raise ValueError("invalid TPK2 magic")
    acts, colors = struct.unpack_from("<BH", data, 4)
    if not 1 <= acts <= 12 or not 1 <= colors <= 255:
        raise ValueError("invalid action/palette count")
    offset = 7 + 2 * colors
    if offset > len(data):
        raise ValueError("truncated palette")
    seen = set()
    idle_supported = False
    for _ in range(acts):
        if offset + 4 > len(data):
            raise ValueError("truncated action header")
        aid, width, height, frames = struct.unpack_from("<BBBB", data, offset)
        offset += 4
        if aid in seen or aid >= 12 or not width or not height or not 1 <= frames <= 24:
            raise ValueError("invalid or duplicate action")
        seen.add(aid)
        length = 2 * frames + width * height * frames
        if offset + length > len(data):
            raise ValueError("truncated action frames")
        if aid == 0:
            # Current LVGL loader is limited to 12 enlarged Idle frames,
            # each within 96x96 and no more than 512KiB in PSRAM total.
            packed = width * height * 9 * 3 * frames
            if frames > 12 or width > 96 or height > 96 or packed > 512 * 1024:
                raise ValueError("Idle exceeds active DIAL renderer limits")
            idle_supported = True
        offset += length
    if offset != len(data) or not idle_supported:
        raise ValueError("trailing bytes or missing supported Idle animation")
    return len(data)


def audit(catalog_file, mons_dir):
    data = json.loads(catalog_file.read_text(encoding="utf-8"))
    if data.get("schema") != 1 or not isinstance(data.get("species"), list):
        raise ValueError("invalid catalog schema")
    installed = {tier: [] for tier in TIERS}
    listed = set()
    rows = []
    for item in data["species"]:
        if not isinstance(item, dict):
            raise ValueError("invalid catalog entry")
        number, tier = item.get("id"), item.get("tier")
        if type(number) is not int or not 1 <= number <= 1025 or number in listed:
            raise ValueError("invalid or duplicate species ID")
        if tier not in TIERS:
            raise ValueError(f"invalid tier for #{number}")
        listed.add(number)
        expected = f"p{number}.bin"
        if item.get("asset") != expected:
            raise ValueError(f"asset path mismatch for #{number}")
        # Never honor arbitrary catalog paths (or symlinks) as asset paths.
        path = mons_dir / expected
        if path.is_symlink():
            raise ValueError(f"symlink forbidden for #{number}")
        if not path.is_file():
            rows.append((number, tier, "MISSING"))
            continue
        try:
            size = validate_tpk2(path)
        except ValueError as e:
            rows.append((number, tier, f"INVALID: {e}"))
            continue
        installed[tier].append(number)
        rows.append((number, tier, f"VALID ({size} bytes)"))
    ready = {
        ball: all(not weight or installed[tier]
                  for tier, weight in weights.items())
        for ball, weights in WEIGHTS.items()
    }
    return rows, ready


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path,
                        default=Path(__file__).with_name("catalog.json"))
    parser.add_argument("--mons-dir", type=Path, required=True,
                        help="Path to the local SD /mons directory")
    args = parser.parse_args()
    try:
        rows, ready = audit(args.catalog, args.mons_dir)
    except (OSError, ValueError, json.JSONDecodeError) as e:
        parser.error(str(e))
    for number, tier, status in rows:
        print(f"#{number:04d} [{tier}] {status}")
    for ball, allowed in ready.items():
        print(f"{ball.upper():6} OPEN: {'READY' if allowed else 'BLOCKED (missing installed tiers)'}")


if __name__ == "__main__":
    main()
