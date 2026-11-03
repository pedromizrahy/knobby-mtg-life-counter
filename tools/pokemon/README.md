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
