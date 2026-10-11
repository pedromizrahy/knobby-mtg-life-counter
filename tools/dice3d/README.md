# DIAL 3D dice — reproducible source

The mesh generator is [Longi94/blender-dice-gen](https://github.com/Longi94/blender-dice-gen) (MIT), vendored here under `vendor/` with its license. This is **actual 3D geometry**, not a 2D illustration rotated in LVGL.

## Generate and compile D6 on Windows

Install Blender 4.x, then run `tools\\dice3d\\BUILD_D6.cmd` from the repository checkout. The batch script discovers Blender, generates `knobby/src/dial_d6_frames.h`, verifies the format signature and invokes Arduino CLI clean compilation **without uploading**.

The new D6 header contains six independent 12-frame 3D roll paths plus six landing frames, with each path landing on the frame used as its result. Output is RGB565, RLE compressed, 196×196, compatible with the existing sprite buffers. The firmware uses the new indexing only when `DIAL_D6_BLENDER_V1` is defined; old V5 headers remain safe.

The rendering pipeline has not yet been tested on the physical device. Check flash partition occupancy (12.5 MiB), rendered legibility, exact landing orientations and animation before uploading.

## Next steps

Use the same vendored mesh definitions to build coherent D4, D8, D10, D12, D20 and percentile dice assets. Render numbers with antialiasing, scale to fit each face, then export with the same RGB565 compression. The existing coin source needs separate cleanup and is **not changed** by this work.

Avoid silently installing headers from prior V-series ZIP packages after generating these frames.
