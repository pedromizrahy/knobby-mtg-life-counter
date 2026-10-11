# DIAL DOS PRIMOS — firmware build / upload (Windows)

The application uses Arduino CLI, ESP32-S3 and a custom **12.5 MiB application partition**. Keep the branch `feat/game-events-turn-manager`.

## Normal workflow

Open **CMD** (not PowerShell), then run:

```cmd
cd /d C:\dev\sistema-mz\knobby-mtg-life-counter
git pull origin feat/game-events-turn-manager
BUILD.cmd
```

The build script:
1. Checks all ten required sprite headers in `knobby\src`. Missing assets **fail the build**.
2. Computes and saves SHA-256s in `.dial-build\assets-build-report.json`. Where a SHA-256 has been pinned in `tools\assets-manifest.json`, it validates the file and can download it from the pinned GitHub release URL.
3. Builds to a fixed `.dial-build\arduino` directory and verifies that `knobby.ino.bin` fits the **0xC80000**-byte partition.
4. Never uploads on its own.

To upload the compiled binary, connect the board and run:

```cmd
UPLOAD.cmd
```

The default serial port is COM4. A different port can be specified by `UPLOAD.cmd -Port COM5`. The uploader checks that sprite files have not changed since the build and flashes the exact compiled binary.

To force a fresh rebuild: `BUILD.cmd -Clean`.

## Important: current asset provenance

**The manifest has no pinned downloads yet.** The large `.h` sprite headers from the previous V-series installers remain **only on the developer's PC**, not in GitHub. This version of BUILD.cmd works with those existing local files, generates a hash report and *warns* that asset versions are not yet reproducible across machines.

Once a known-good set has been accepted on hardware, publish the exact headers under a versioned GitHub Release and fill `locked` in `tools/assets-manifest.json` with names, release URLs and SHA-256 checksums. Subsequent pulls will fetch any missing files and **reject mismatches**. Do not invent a release URL or checksum.

The new build scripts do not replace the actual graphics, modify the V9 dice artwork, or require Blender/Python locally. Their purpose is to stop accidentally combining firmware with missing or changed graphical assets.
