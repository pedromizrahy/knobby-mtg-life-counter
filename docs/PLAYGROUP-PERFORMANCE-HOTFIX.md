# Playgroup performance hotfix — 2026-11-07

Symptom observed on hardware: `PG commander prepare total=73012 ms
decks=5 failures=0`, followed by reset. Earlier Loading Account spent
25,402 ms including a pending match synchronization.

## Changes
- The Playgroup account picker no longer synchronizes pending games
  synchronously. **All queued snapshots remain stored** and the existing
  USB command `PG SYNC PENDING` still performs explicit synchronization.
  This is a temporary UX trade-off pending a safe background/UI sync flow.
- After a player's `GET /decks` succeeds, the deck picker opens
  immediately with deck/commander names. It does not wait for the
  previous five-JPEG bulk preload and decode worker.
- The selected commander art is requested lazily by the existing timer;
  browsing/selecting works even if art has not arrived. Cached JPEGs
  are reused. If selected too quickly, game art may be absent and color
  fallback remains valid (do not fabricate or block selection).
- The decoded JPEG cache is limited to two entries and the active LVGL
  image source is detached **before** freeing a cache entry.
- A post-listing log includes internal heap/PSRAM free and largest
  internal allocation to help diagnose low-memory conditions.
- Add source-level regression tests for fast path and existing
  `PG SYNC PENDING` behavior.

## Verification
CI Arduino build and Linux tests are necessary but not sufficient.
On real hardware check: Loading Account, selecting Playgroup, selecting
each player, opening decks, changing decks, pressing SELECT, continuing
to mulligans, and recovering after power cycle.

If it resets again, record serial logs from BEFORE and AFTER reset,
including `Guru Meditation`, `Backtrace`, `Brownout`, and `[Diag]`
lines. Current reset UNKNOWN (11) / abnormal count alone does not
identify the root cause.
