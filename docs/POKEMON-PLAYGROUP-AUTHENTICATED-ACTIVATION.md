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
- `PK STARTER ME` — currently **PAUSED**, prints an explanation and
  creates no Pokémon. The former fixed Mimikyu starter is retired.
  The replacement will be an initial Pokéball roll plus five rerolls,
  with the chance table of a regular Pokéball for every reveal.
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

## Starter draw policy (pure engine, not yet enabled for real profiles)
- One initial Pokéball-tier reveal plus five rerolls (six candidates).
- Each reveal uses the regular Pokéball tier weights: 75% common,
  20% rare, 4.5% epic, 0.5% legendary; independent shiny 1/4096.
- KEEP or REROLL after each reveal: the current result is the only selectable result.\n- Each reroll permanently discards the preceding Pokémon; no gallery or picking past rolls.\n- On the sixth reveal the only remaining action is KEEP.
- New `pokemon_starter_roll` pure sequential state machine and tests; **not wired to UI**.
- Next task: durable generation/selection transaction before showing a
  reveal so power loss or reboots cannot reset rerolls or swap choices;
  integrate actual installed SD sprite validation and touch UI.

## Durable starter state backend (isolated, not activated yet)
- `pokemon_starter_store.cpp` writes two authenticated-user-ID-keyed SPIFFS
  records with checksums, generations and full revealed-species history.
- Explicit create requires exactly the first opening. Explicit update
  accepts only the next sequential reroll or KEEP of the current result.
- No silent rollback on corrupt partial writes: returns CORRUPT until
  diagnosed manually, so rebooting cannot replenish rerolls.
- The backend never formats SPIFFS, creates no profile, touches no SD,
  and currently has **no production call site**. Visual KEEP/REROLL
  and the final profile-creation transaction remain to be implemented.

## Selected-starter transaction (opt-in integration, not yet hooked to UI)
- `pk_starter_service_open` reads an interrupted reveal or creates exactly
  one initial stored roll when BOTH NVS and SPIFFS profile stores are empty.
- `pk_starter_service_reroll` generates the next candidate only after the
  previously committed roll is loaded and profile stores are still empty.
- `pk_starter_service_keep` writes the KEEP decision before making the
  selected Pokémon's real level-1 profile, and verifies the new profile.
- Retrying after an interrupted KEEP checks the existing profile against
  the saved species and shiny flag. No overwrite or reroll restoration.
- The UI authentication gate, real sprite preflight and touchscreen
  activation are NOT wired yet; no automatic new profile or reward.
