#!/usr/bin/env python3
"""Prepare local TPK2 Idle assets from public PokeAPI sprite URLs.

Sprite images are third-party Pokemon/Smogon artwork; not bundled or
licensed by this project. Check rights before distribution. User chooses
to download them locally; nothing is committed or sent to the DIAL.

python tools/pokemon/prepare_sprites.py --out-dir assets/pokemon --species 25 149 150
python tools/pokemon/upload_batch.py --mons-dir assets/pokemon       # dry run
python tools/pokemon/upload_batch.py --mons-dir assets/pokemon --execute --port COM4
"""
import argparse
import io
import json
import struct
import urllib.error
import urllib.request
from pathlib import Path

from PIL import Image
from audit_catalog import validate_tpk2

SOURCE = "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/"
MAX_FRAMES = 6
SIDE = 48
COLORS = 240
MAX_BYTES = 512 * 1024


def fetch_sprite(number):
    """Prefer the animated Showdown front view, then the Gen5-style PNG."""
    for suffix in (f"other/showdown/{number}.gif", f"{number}.png"):
        url = SOURCE + suffix
        try:
            with urllib.request.urlopen(urllib.request.Request(
                url, headers={"User-Agent": "DIAL-local-TPK2-preparer/1.0"}),
                timeout=20) as response:
                data = response.read(2 * 1024 * 1024 + 1)
            if len(data) > 2 * 1024 * 1024:
                raise ValueError("Image download exceeds 2 MiB")
            with Image.open(io.BytesIO(data)) as img:
                img.verify()
            return data, url
        except (urllib.error.HTTPError, urllib.error.URLError, OSError):
            continue
    raise FileNotFoundError(f"No sprite source for #{number}")


def frames_from_image(data):
    with Image.open(io.BytesIO(data)) as image:
        length = getattr(image, "n_frames", 1)
        # Deterministic samples across the animation, max 6 frames.
        picks = sorted({int(i * (length - 1) / max(1, min(length, MAX_FRAMES)-1))
                        for i in range(min(length, MAX_FRAMES))})
        frames, durations = [], []
        for index in picks:
            image.seek(index)
            original = image.convert("RGBA")
            # Fit within 48x48 canvas, preserve proportions and nearest pixels.
            original.thumbnail((SIDE, SIDE), Image.Resampling.NEAREST)
            canvas = Image.new("RGBA", (SIDE, SIDE), (0, 0, 0, 0))
            canvas.alpha_composite(original, ((SIDE-original.width)//2,
                                              (SIDE-original.height)//2))
            frames.append(canvas)
            duration = image.info.get("duration", 150)
            durations.append(max(70, min(800, int(duration or 150))))
    if not frames:
        raise ValueError("No animation frames")
    return frames, durations


def pack_tpk2(frames, durations):
    count = len(frames)
    if not 1 <= count <= MAX_FRAMES or len(durations) != count:
        raise ValueError("Invalid Idle animation")
    # One shared palette for all frames; transparent pixels become 0xff.
    atlas = Image.new("RGB", (SIDE, SIDE * count))
    for i, frame in enumerate(frames):
        atlas.paste(frame.convert("RGB"), (0, SIDE*i))
    indexed = atlas.quantize(colors=COLORS, method=Image.Quantize.MEDIANCUT,
                             dither=Image.Dither.NONE)
    palette = indexed.getpalette()
    palette_words = []
    for i in range(COLORS):
        r, g, b = palette[3*i:3*i+3]
        palette_words.append(((r >> 3)<<11)|((g >> 2)<<5)|(b >> 3))
    parts = [b"TPK2", struct.pack("<BH", 1, COLORS),
             struct.pack("<" + "H"*COLORS, *palette_words),
             struct.pack("<BBBB", 0, SIDE, SIDE, count),
             struct.pack("<" + "H"*count, *durations)]
    ids = indexed.tobytes()
    for i, frame in enumerate(frames):
        alpha = frame.getchannel("A").tobytes()
        chunk = bytearray(ids[i*SIDE*SIDE:(i+1)*SIDE*SIDE])
        for p, a in enumerate(alpha):
            if a < 128:
                chunk[p] = 255
        parts.append(bytes(chunk))
    data = b"".join(parts)
    # Firmware expands every pixel 3x in RGB565A.
    if SIDE * SIDE * 9 * 3 * count > MAX_BYTES:
        raise ValueError("Decoded Idle exceeds firmware PSRAM budget")
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--catalog", type=Path,
                        default=Path(__file__).with_name("candidate_pool.json"))
    parser.add_argument("--species", type=int, nargs="*",
                        help="National Dex IDs; default all candidates except Mimikyu")
    parser.add_argument("--overwrite", action="store_true",
                        help="Overwrite local generated files ONLY (never #778)")
    args = parser.parse_args()
    manifest = json.loads(args.catalog.read_text(encoding="utf-8"))
    allowed = {r["id"] for r in manifest["species"] if r["id"] != 778}
    selected = set(args.species) if args.species else allowed
    if not selected or not selected.issubset(allowed):
        parser.error("Unknown or excluded species (Mimikyu #778 cannot be replaced)")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    results = []
    for number in sorted(selected):
        output = args.out_dir / f"p{number}.bin"
        if output.exists() and not args.overwrite:
            print(f"#{number}: exists, skipped (no overwrite)")
            continue
        try:
            blob, url = fetch_sprite(number)
            frames, durations = frames_from_image(blob)
            encoded = pack_tpk2(frames, durations)
            temp = output.with_suffix(".tmp")
            temp.write_bytes(encoded)
            try:
                validate_tpk2(temp)
                temp.replace(output)
            finally:
                temp.unlink(missing_ok=True)
            print(f"#{number}: {len(frames)} frames, {len(encoded)} bytes -> {output}")
            results.append({"id": number, "url": url, "frames": len(frames)})
        except (OSError, ValueError, urllib.error.URLError) as exc:
            print(f"#{number}: FAILED: {exc}")
    # Source metadata only, deliberately no binaries committed to Git.
    report = args.out_dir / "sprite_sources.json"
    report.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print("Local output only. Validate with upload_batch.py before transfer.")


if __name__ == "__main__":
    main()
