# Visual assets V5 (local install)

The rendered RGB565 / RLE frame headers live in the Windows build workspace, not in this Git branch.

V5 updates these local files:
- knobby/src/dial_d6_frames.h
- knobby/src/dial_d8_frames.h
- knobby/src/dial_d10_frames.h
- knobby/src/dial_d12_frames.h
- knobby/src/dial_d100t_frames.h
- knobby/src/dial_d100u_frames.h
- knobby/src/dial_coin_frames.h
- knobby/src/dial_intro_asset.h

The GitHub code fix in commit d9b239d ensures the introduction displays at native scale. V5 local installer must be run after git pull. D4 and D20 frames are intentionally unchanged. DO NOT assume a git pull alone installs the V5 frames.

To build, use the installer batch file from the V5 artifact; it installs frame headers and compiles without uploading.

Visual QA: inspect the final D6 cube, contrast and angle of D10 numerals, and the lower border of HEADS/TAILS on the physical 360px screen.
