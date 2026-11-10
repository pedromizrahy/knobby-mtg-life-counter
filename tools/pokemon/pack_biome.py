#!/usr/bin/env python3
"""Convert user-owned/licensed square scenery PNG/JPEG to DIAL PKBG.
The resulting .pkbg is a native 180x180 RGB565 resource for a 2x LVGL zoom.
Nothing is uploaded, fetched from the internet or inserted into firmware.
Requires Pillow: py -3 -m pip install pillow
"""
import argparse
from pathlib import Path
import struct

from PIL import Image, ImageOps

BIOMES = ("meadow", "forest", "sea", "volcano", "cave", "haunted")
WIDTH = HEIGHT = 180


def convert(source: Path, output: Path):
    with Image.open(source) as original:
        image = ImageOps.fit(original.convert("RGB"), (WIDTH, HEIGHT),
                             method=Image.Resampling.NEAREST, centering=(0.5, 0.5))
        pixels = image.load()
        payload = bytearray(b"PKBG" + struct.pack("<HH", WIDTH, HEIGHT))
        for y in range(HEIGHT):
            for x in range(WIDTH):
                r, g, b = pixels[x, y]
                rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                payload += struct.pack("<H", rgb565)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)
    print(f"Created {output} ({len(payload)} bytes; {WIDTH}x{HEIGHT}, RGB565 LE)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--biome", required=True, choices=BIOMES)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path,
                        help="Destination /scenes/pokemon on the SD card")
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"Image not found: {args.input}")
    output = args.output_dir / f"{args.biome}.pkbg"
    if output.exists():
        parser.error(f"Refusing to overwrite existing scene: {output}")
    convert(args.input, output)


if __name__ == "__main__":
    main()
