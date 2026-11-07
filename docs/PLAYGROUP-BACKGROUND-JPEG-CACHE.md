# Five-deck background JPEG predownload — 2026-11-07

Observed regression: deck selection took 73 seconds because five
300×219 JPEGs were downloaded/decoded before the picker opened.
This change **preserves the immediate text-only picker** from
`5ff487c` and restores fast, persistent artwork loading separately.

## User-facing behavior
- **Five most-used decks** appear first; **MORE DECKS** gives the rest.
- After GET /decks returns, the UI opens immediately with names. A
  low-priority worker fetches up to five **compressed JPEGs** into
  the existing SPIFFS art cache without decoding all five into PSRAM.
- Images already cached are skipped. One worker queues bounded jobs
  for up to four Playgroup members during setup, instead of launching
  multiple concurrent downloads; the selected card's image gets priority.
- Selecting a deck outside the top five may fetch and persist its
  own artwork on demand.
- No image download is required to press SELECT and proceed. Gameplay
  does not depend on completed art prefetch.

## Storage boundaries
- Existing art files are stored in ESP32 **SPIFFS flash** (`/pga_*.bin`),
  *not* in the microSD. The app partition reserves 3.375 MiB for
  SPIFFS; the SD card's approx. 300 MiB free is a separate device.
- Preserve at least 512 KiB free in SPIFFS for Pokémon profiles and
  pending Playgroup games. If insufficient, skip saving the image:
  never delete those records to make room for a JPEG.
- No SPIFFS auto-format on cache mount failure. No SD changes.
- Compressed in-RAM cache reduced from eight to three entries, and
  the decoded art cache in the picker remains limited to two.
  No five-image JPEG decode batch.
- Queued downloads cancel when the Playgroup setup session ends.
  Do not reset the shared artwork TLS client while its network mutex
  is held by an in-flight job.

## Verification
- Source-level integration checks are not hardware tests.
- Confirm repeated visits to the same players' deck lists show
  `Commander art flash cache hit` and fewer/shorter loading stalls.
- If the device still resets, collect complete serial output around
  Guru Meditation / Backtrace / reset reason, not just last ten lines.
