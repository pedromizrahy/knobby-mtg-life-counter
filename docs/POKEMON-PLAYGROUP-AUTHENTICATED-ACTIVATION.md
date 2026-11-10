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

## Touchscreen starter activation in Playgroup PARCEIROS
- Tap a NOT LINKED player card. The preview uses the original numeric
  Playgroup identity; it refuses guest/duplicate/unmapped IDs.
- Fresh authenticated `GET /me` must match the tapped ID exactly.
- The DIAL scans installed SD sprites and constructs a Pokéball rarity
  catalog from available species, refusing absent rarity tiers.
- Opening and each reroll are persisted **before** the Pokéball animation.
- KEEP persists the final choice before creating the selected real profile.
- After exiting, PARCEIROS re-reads the actual saved profiles and sprites.
- If sprite decoding fails, KEEP is disabled, and the user can exit safely.
- This implementation is ready for CI/hardware validation; do not infer
  success on physical touch/SD/TLS merely from compilation.

## Physical round-display fix (2026-11-07)
- Photo of the physical DIAL confirmed header/counter overlap, BACK over
  the sprite, and name/status touching action buttons.
- The layout now reserves separate one-line rows for title, counter,
  BACK, sprite, species, tier, status, and KEEP/REROLL.
- Rarity styling: COMMON white, RARE blue, EPIC purple,
  LEGENDARY orange. Rarity fades in and the sprite pops to full size
  before KEEP and REROLL become interactive.
- These are presentation-only changes. Existing revealed Furret #162
  and its saved 5 rerolls must persist unchanged on upgrade.
- TODO: evolution chains and gacha base-form-only restriction; preserve
  any pre-existing roll, including evolved species from the old catalog.

## Temporary developer account reroll reset
- The literal serial command `PK TEST STARTER RESET 184199 CONFIRM`
  re-enables experimentation with a full 5/5 rerolls, **only** for global
  Playgroup ID 184199 after a fresh authenticated `GET /me`.
- Refuses during active games, if a real SPIFFS/NVS Pokémon profile exists,
  or if the starter has already been KEEP-confirmed.
- The storage function is hardcoded to that ID, removes only its two
  `/psr_0002CF87a.bin` / `/psr_0002CF87b.bin` starter records, and never
  formats, erases or resets other files or other users. Note: paths are
  created by the formatter from the numeric ID; don't run manual deletion.
- No starter/profile was reset merely by deploying this firmware.

## Evolution foundations and first-stage-only NEW starter draws
- Static offline data derived from PokeAPI public species/evolution CSV
  (generation I/II subset, species numbers 1..251).
- New starter draws are restricted to the 122 true base species with
  no known pre-evolution anywhere in National Dex; SD sprites must exist.
  A saved earlier Furret or another already-revealed evolved species
  remains valid for KEEP, even after this update.
- Query evolution branches by source species: level-up, stone, trade,
  friendship or special. The 83 plain level-only steps use the official
  minimum levels. Condition-specific rules do not become free level-ups.
- Pure `pk_evo_prepare_level` creates a modified *copy* and preserves level,
  XP, shiny, team and Dex. No production profile mutations or automatic
  evolution have been connected yet.
- NEXT: install offline items dictionary and per-Pokemon milestone inventory
  with one-time, crash-safe claims; prompt EVOLVE on touchscreen, verify
  target SD sprite, and commit confirmed profile as one durable transaction.

## Evolution inventory prototype (not yet wired to real profiles)
- Pure `pokemon_evolution_items` defines once-per-Pokémon claims at
  levels 30 (one selected stone), 50 (one selected special item),
  and 75 (one item of either category).
- Supported prototype items: Fire/Water/Thunder/Leaf/Moon/Sun Stones,
  Link Cable, Metal Coat, King's Rock, Dragon Scale and Up Grade.
- Items are stored in a whole-ledger inventory; an immutable **instance ID**
  identifies a Pokémon across team reordering and evolution. A single
  level milestone marks one earned claim, and claiming it credits inventory
  in the same state transformation. Spending decrements a selected item.
- The current version-1 profile does **not** have an immutable Pokémon
  instance ID. DO NOT bind claims to species or team index, and DO NOT
  change serialized profile structs in place (would invalidate existing
  SPIFFS and NVS checksums/record sizes).
- This module has isolated unit tests; no auto-credit from battle/XP, no
  persistent inventory and no touchscreen item spending are active yet.
  Next: safely migrate stable IDs, design an atomic versioned ledger store,
  and connect verified saved battles to earned milestones.

## Persistent item sidecar (new isolated storage module)
- A versioned record `PKI1` is stored in two per-Playgroup-account
  SPIFFS files (`pki_<account>a/b.bin`). FNV checksum plus generation
  enable safe verification after writes and compare-and-swap updates.
- A damaged or partial slot blocks all changes; the firmware MUST NOT
  fall back to an older inventory or automatically wipe the sidecar.
  Existing account profiles (SPIFFS `pkf_*` and NVS) are unchanged.
- A previously registered immutable Pokémon ID cannot be replaced;
  past earned/claimed flags cannot be cleared in a normal update.
- This storage API intentionally has **no automatic initialization**:
  no reliable permanent Pokémon ID exists in version-1 profiles yet.
  Only an explicitly authenticated, safely bound starter instance may
  enroll. The immutable instance ID must survive party reorder, XP gains,
  and evolution.
- NEXT: guarded one-time enrollment for a confirmed KEEP starter; account
  proof from fresh /me; link the saved instance ID with the Pokémon
  independently of mutable species/level; no reward claims until verified.

## Explicit confirmed-starter identity enrollment (still no live UI hook)
- `pk_item_enroll_confirmed_starter` binds a securely generated nonzero
  64-bit instance ID to exactly one confirmed KEEP starter. Enrollment
  checks the saved KEEP decision, canonical SPIFFS profile, species and
  shiny agreement, and absence of a legacy NVS record.
- Repeated calls return the already-registered identity rather than
  granting new milestones or rerolling identity. Existing unreadable or
  corrupt item sidecars block enrollment.
- Enrollment copies earned level milestones but never grants item credit
  automatically. No mutation occurs to version-1 Pokemon profiles.
- The caller still MUST fresh-authenticate /me and pass only its ID;
  function is not yet wired to touchscreen or a serial command. The
  secure random 64-bit ID must be supplied by the future controller.
- TODO: bind IDs to Pokémon over reorder and future captures (not merely
  slot 0), integrate verified level progression and crash-safe choice
  UI, then evolution confirmation and item spending in one transaction.

## Starter evolution milestones coordinator (no activation yet)
- `pk_item_reward_sync_starter` reads only a verified, confirmed
  KEEP starter and its canonical saved level, then persists earned bits
  at 30/50/75 without crediting an item automatically.
- `pk_item_reward_claim_starter` accepts a selected valid item, verifies
  the milestone against the canonical level, and atomically writes
  both the claim bit and inventory increase in one sidecar revision.
- This deliberate starter-only gate refuses extra team members,
  species/shiny mismatch, missing item sidecar and legacy NVS;
  evolved Pokémon and new captures need a stable mapping first.
- Neither routine is called by gameplay, serial or UI; they require
  fresh authenticated `/me` at the future controller entry point.
- CI simulates authenticated-v1 state, crossing milestones,
  duplicate claims, and persisted readback.

## Read-only physical EVOLVE screen (preview only)
- From Playgroup -> POKEMON -> linked own player card, a fresh /me
  numeric identity check gates the EVOLVE preview. Unlinked cards
  still open the unchanged starter Pokeball/reroll flow.
- Circular 360x360 screen shows active canonical Pokémon,
  its current level/shiny, first installed TPK2 Idle sprite, paginated
  evolution targets and their visible requirements; NEXT/BACK never
  grants or removes Pokémon or inventory.
- Requirement labels: canonical evolution level; known Kanto/Johto
  stones and held-item/link distinctions; friendship and special
  conditions are explicitly pending. Presenting an option does NOT
  imply it is currently executable.
- Inventory is read-only from the separate sidecar; a missing sidecar
  displays zero items. The UI does not initialize or migrate accounts.
- The screen deliberately has NO evolve/confirm or item-spend action.
  A future transaction must commit profile species + item consumption
  with crash-safe recovery and preserve immutable instance ID before
  granting an evolution.

## Level evolution write-ahead transaction coordinator (pure engine)
- The new `pokemon_evolution_transaction` module is designed for a
  player-confirmed, LEVEL-only evolution. A persisted intent includes
  the full old profile, next profile, immutable target and transaction
  nonce; the mutation must be saved conditionally against the complete
  old profile to prevent losing new XP or other concurrent changes.
- Crash/restart recovery accepts only exactly the old or new snapshot.
  It resumes a pending consented transition, never re-applies an already
  written evolution, and refuses any unexpected profile divergence.
- Item evolution remains blocked: species + item spending needs a
  separate cross-record transaction with recovery and idempotency.
- This is an isolated tested coordinator; there is not yet a concrete
  durable write-ahead storage adapter, atomic save-if-unchanged function
  or enabled CONFIRM button. No real user profile is changed here.
