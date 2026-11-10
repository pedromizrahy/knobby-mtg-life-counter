# Pokémon scenery resources (optional)

The companion can now show **real illustrated scenery** instead of the
procedural background. It searches the microSD for the following files:

| Biome | microSD path |
|---|---|
| Meadow | `/scenes/pokemon/meadow.pkbg` |
| Forest | `/scenes/pokemon/forest.pkbg` |
| Sea | `/scenes/pokemon/sea.pkbg` |
| Volcano | `/scenes/pokemon/volcano.pkbg` |
| Cave | `/scenes/pokemon/cave.pkbg` |
| Haunted | `/scenes/pokemon/haunted.pkbg` |

When an optional resource is missing or invalid, the firmware draws the
built-in original pixel-art scene for that biome. Every read is read-only,
and the card is unmounted immediately afterwards. No asset is downloaded
automatically. Magic gameplay assets, NVS, Playgroup and the Mimikyu TPK2
file remain untouched.

## Format

`PKBG` (4 bytes), little-endian width/height (2+2 bytes, each exactly
180), then 32,400 little-endian RGB565 pixel values (64,808 total bytes).
The firmware converts each pixel to LVGL native colors, including support
for the board's `LV_COLOR_16_SWAP` setting. It displays the 180px image
at 2x for the 360px screen. The image should already be composed with a
safe area around its edges for a circular display, and the central Pokémon
must remain visible against it.

## Prepare an image you are licensed to use

Install Pillow locally:

```powershell
py -3 -m pip install pillow
```

Example for a locally saved pixel-art volcano image:

```powershell
py -3 tools\pokemon\pack_biome.py --biome volcano --input C:\dev\pokemon-art\volcano.png --output-dir E:\scenes\pokemon
```

Replace `E:` with the actual microSD drive letter. The converter
refuses to overwrite an existing file, does not download images, and
does not modify firmware. **No image resources or artist font files are
included in this repository.** Respect each artist's actual license
and attribution requirements when supplying artwork. Never use Pokémon
game screenshots as redistributed standalone assets.

This optional workflow requires access to the SD card filesystem and
therefore should only be used when the user is able to copy files to it.
There is no USB scene upload protocol in this revision.
