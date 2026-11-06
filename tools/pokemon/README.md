# Mimikyu #778 — TamaPoke/PMD assets for DIAL DOS PRIMOS

This workflow uses **real sprites** (not LVGL placeholder artwork). It keeps
Pokémon art **outside our firmware repository**. The original game engine is
[socquique/TamaPoke](https://github.com/socquique/TamaPoke), and the sprite
artwork comes from [PMDCollab/SpriteCollab](https://github.com/PMDCollab/SpriteCollab).

The TPK2 format is documented in TamaPoke's `tools/pack_pmd.py`. The upstream
packer can produce `p778.bin` with idle, walking, sleeping and other animations.

## Prepare asset locally (PowerShell, Windows)

```powershell
cd C:\dev
git clone https://github.com/socquique/TamaPoke.git
py -3 -m pip install pillow pyserial
cd C:\dev\TamaPoke
py -3 tools/pack_pmd.py normal 778
Test-Path .\tools\sdcard\mons\p778.bin
```

The last line must print `True` and the packer must report `p778.bin`.
If generation fails, **do not** upload or create placeholder files.

## DIAL firmware

After main CI succeeds, run the local project's normal `git pull`, BUILD.cmd,
UPLOAD.cmd sequence. Verify that Magic still starts normally. The Pokémon
commands are only enabled **outside active Playgroup games**.

## Transfer sprite over USB-C without opening the case

Close the Arduino serial monitor first. From the DIAL repository:

```powershell
cd C:\dev\sistema-mz\knobby-mtg-life-counter
py -3 tools\pokemon\upload_mimikyu.py --port COM4 --file C:\dev\TamaPoke\tools\sdcard\mons\p778.bin
```

- Only writes to `/mons/p778.part` while receiving chunks, then renames to
  `/mons/p778.bin` after the whole transfer matches CRC32.
- **Existing p778.bin files are never overwritten or deleted.**
- Transfer is manually initiated; it does not run on boot.
- If serial port is not COM4, substitute the port shown by Windows.

Then re-open the Arduino serial monitor (115200 baud), run:

```text
PK PMD CHECK
PK SD STATUS
```

Expected when asset is present: `PMD VALID`, action/frame counts, and
`mimikyu_tpk2=YES`. These commands are read-only.

**Important:** this phase only installs and validates the actual multi-frame
sprite asset. It **does not render animation yet**; that needs a dedicated LVGL
renderer, which should be developed and tested separately.

## Credits / terms

TamaPoke source code MIT; source artwork credited to SpriteCollab artists,
generally under CC BY-NC 4.0. See upstream CREDITS.md and per-species
tracker.json. Use for private, noncommercial projects with appropriate credit;
do not commit or commercially redistribute downloaded sprites.


## Gacha catalog audit (read-only, no firmware upload)

A separate `catalog.json` lists only artwork already confirmed in the current
project. **It currently lists Mimikyu #778 only**; this is *not* a ready-to-open
gacha pool. Its `rare` tier is a **provisional DIAL game-design category**, not
an official Pokémon rarity. No additional species are assumed installed.

On Windows, copy the installed SD `/mons` directory to a local folder
(or prepare a local mirror using your own properly licensed TPK2 assets).
Then run:

```powershell
cd C:\dev\sistema-mz\knobby-mtg-life-counter
py -3 tools\pokemon\audit_catalog.py --mons-dir "C:\path\to\sdcard\mons"
```

The report indicates VALID, MISSING, or INVALID for each listed sprite and
whether OPEN is READY or BLOCKED for Poké, Great, Ultra and Master Balls.
It is **only a local audit**; the current DIAL firmware does not read this
manifest or use it to issue real rewards. The audit intentionally never
writes to the SD card, does not install sprites, and does not create new
species/art. A gacha opening is disabled if a tier with positive probability
has no valid installed sprite.

Tests (pure standard-library Python):

```powershell
py -3 -m unittest discover -s tests -p test_pokemon_catalog.py -v
```

To expand the pool, add a legitimate species entry with `id`, `name`,
`tier`, and exactly `p<ID>.bin` under `asset`; then install and validate
that TPK2 file. Current renderer is still hardcoded to load Mimikyu
`/mons/p778.bin`, so the audit **does not imply** newly listed Pokémon will
render in the companion screen. Generic species loading, SD transfer, and
persistent gacha opening are future steps. Respect the original artwork's
licensing before distribution.
