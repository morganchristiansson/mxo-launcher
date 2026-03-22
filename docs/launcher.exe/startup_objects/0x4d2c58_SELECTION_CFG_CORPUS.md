# `ILTLoginMediator.Default` non-`mcd.cfg` selection cfg corpus subset

Focused canonical note for the client-side per-selection cfg corpus that is **not** `mcd.cfg`.

Use this doc for the remaining gaps such as:
- skills / bonus skills not loading
- Hyper-Jump / Hyper-Speed / Hyper-Sprint actions missing
- equipment/loadout parity still incomplete

Keep `0x4d2c58_MCD_CFG_PERSISTENCE.md` narrowly about the already-closed `mcd.cfg` path.

## Purpose

Track the client-side helper family that sits before the already-proven mediator-backed `mcd.cfg`
adopt/save path and appears to load or synthesize the rest of the per-character / per-selection cfg
corpus.

Current best read:
- `mcd.cfg` parity is now closed
- the next missing launcher/arg6 fidelity surface is the broader selection cfg corpus rooted in the
  client helper family at:
  - `+0x68 .. +0x90` bool-style gates
  - `+0x94 .. +0xb8` pointer/out-length getters

## Proven client helper family

On the active save-side family rooted at `client.dll:0x62199ed0`, the client calls this series before
reaching the mediator-backed `mcd.cfg` adopter/writer:

- `0x62198670` uses mediator `+0x68`, then `+0x94`
- `0x62198770` uses mediator `+0x6c`, then `+0x98`
- `0x62198870` uses mediator `+0x70`, then `+0x9c`
- `0x62198970` uses mediator `+0x74`, then `+0xa0`
- `0x62198a70` uses mediator `+0x78`, then `+0xa4`
- `0x62198b70` uses mediator `+0x7c`, then `+0xa8`
- `0x62198c60` uses mediator `+0x80`, then `+0xac`
- `0x62198d50` uses mediator `+0x84`, then `+0xb0`
- `0x62198e50` uses mediator `+0x88`, then `+0xb4`
- `0x621993d0` uses mediator `+0x90`, then `+0xb8`

Current replacement behavior is deliberately narrow there:
- each bool gate returns `0`
- each paired getter returns `NULL` and zero length
- logs label these as:
  - `HasLiveCorpus..`
  - `GetLiveCorpus..`
- practical effect: let the client take its fallback/default path and continue into the already-proven
  `mcd.cfg` branch instead of crashing

## Why this family is now the best next suspect

`mcd.cfg` now writes correctly and matches the original bit-for-bit, but the replacement still shows
runtime parity gaps that fit missing non-`mcd.cfg` corpus state much better than missing auth or broad
state9 work.

Current symptom cluster:
- skills / bonus skills missing
- Hyper-Jump / Hyper-Speed / Hyper-Sprint actions missing
- equipment/loadout still incomplete

That makes this pre-`mcd.cfg` live-corpus family the highest-value next RE surface.

## Original on-disk reference corpus

Original current-character folder currently observed at:
- `~/.wine/drive_c/users/morgan/AppData/Local/The Matrix Online/Profiles/morgan/Morg4n_6DCE/`

Representative file sizes there:
- `ai.cfg`  = `30`
- `an.cfg`  = `66`
- `btl.cfg` = `4`
- `cl.cfg`  = `306`
- `cs.cfg`  = `4510`
- `hl.cfg`  = `790`
- `mcd.cfg` = `1218`
- `pi.cfg`  = `270`
- `rl.cfg`  = `1704`

## Current replacement state8 section-size correlations

The replacement already materializes several state8 reply sections with sizes that strongly resemble
those non-`mcd.cfg` files:

- section `3`  = `270`
- section `4`  = `30`
- section `6`  = `790`
- section `7`  = `66`
- section `8`  = `1704`
- section `9`  = `306`
- section `5`  = `2706`
- section `10` = chunked / assembled separately

Current working hypothesis only, not yet closed fact:
- section `3`  ↔ `pi.cfg`
- section `4`  ↔ `ai.cfg`
- section `6`  ↔ `hl.cfg`
- section `7`  ↔ `an.cfg`
- section `8`  ↔ `rl.cfg`
- section `9`  ↔ `cl.cfg`
- `cs.cfg` likely involves section `5` and/or chunked section `10`

Keep these as hypotheses until a narrower client-reader / launcher-producer link is pinned down.

## Active source ownership after cleanup split

Focused replacement source files:
- `src/launcher_mediator_abi_selection_cfg.cpp`
  - non-`mcd.cfg` corpus fallback gates/getters `+0x68 .. +0xb8`
- `src/launcher_mediator_abi_profile_paths.cpp`
  - neighboring profile-path / current-slot / selection-descriptor surface `+0x3c/+0x40/+0x44`
- `src/launcher_mediator_abi_mcd.cpp`
  - already-closed `mcd.cfg` surface
- `matrixstaging/game/src/libltclientlogin/loginstate_state8.cpp`
  - real producer-side state8 section materialization
- `matrixstaging/game/src/libltclientlogin/loginmediator.h`
  - owner-side raw state8/load-character storage layout

Implementation note:
- the focused ABI files are currently split as separate source-owned implementation files included by
  the broader `src/launcher_mediator_abi.cpp` shell, so future RE passes can stay on one surface
  without rereading the whole ABI file

## Practical next step

For the next focused pass:
1. isolate the first exact helper pair in `+0x68 .. +0xb8` that corresponds to one real cfg corpus
2. tie it back to one concrete state8 producer section/buffer
3. implement live mediator-backed data there instead of the current `0` / `NULL` fallback
4. only then check whether the in-game symptom moved

Do not reopen broad auth/bootstrap or generic state9 history unless this exact corpus family forces it.
