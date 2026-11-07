# Playgroup Pokémon: authenticated account activation (opt-in)

The multi-player `PARCEIROS` screen already reads saved profiles by the
Playgroup global numeric user ID (not the seat, local nickname or sprite).
`NOT LINKED` means that no valid principal could be loaded for that seat.
A missing profile is **not** a network error, and other players cannot
be silently provisioned with the API key of the current device owner.

## New USB commands (outside an active game)
- `PK ME` — read-only. Performs a **fresh authenticated GET /me**, checks
  that the global user ID is representable, inspects **both** SPIFFS and
  legacy NVS, and reports linked, legacy, ready, or blocked without writes.
- `PK STARTER ME` — explicit opt-in. Also authenticates via fresh
  GET /me; creates **one** Level-1 Mimikyu #778 in SPIFFS **only if
  both storage backends are genuinely empty**. Read-back verifies it.
  No auto-start on boot or merely opening the Playgroup selector.
  No Pokéball draw, fake gacha outcome, XP, wins or rewards.
- Both commands reject during a live tracked game. A different player
  cannot be provisioned by selecting their seat; they must independently
  authenticate with their own Playgroup API key in a safe session.
- If any backend reports corrupt or read error, **fail closed**.
  If legacy NVS exists, preserve and display it, no automatic migration.
  If SPIFFS exists, preserve that canonical profile. No profile is
  overwritten, cleaned, formatted or silently reset.

## Next UI and game phases
1. Exercise `PK ME` on hardware and inspect the current owner result.
2. Only with deliberate opt-in, run `PK STARTER ME` (Mimikyu starter).
3. Reopen PLAYERS & DECKS -> POKEMON; one valid linked profile should
   display its main sprite, name, and LV from the saved profile.
4. Later, implement a choice of actual starter/gacha ownership mechanics,
   persistent PC, Dex and team management, confirmed game XP integration,
   and user-friendly account-switching/consent UI. None is enabled here.

Security boundaries: no SD writes, no SPIFFS auto-format, no legacy
migration, no automatic starter for other players, no Playgroup
game creation, no automatic credits/rewards. Existing draft PR remains
unmerged. Arduino CI does not validate the physical UI.
