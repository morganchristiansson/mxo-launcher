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

Current replacement behavior is now split deliberately:
- exact client/launcher/state8 mappings are now closed for the full non-`mcd.cfg` helper family:
  - `+0x68/+0x94` = `hl.cfg`
  - `+0x6c/+0x98` = `an.cfg`
  - `+0x70/+0x9c` = `pi.cfg`
  - `+0x74/+0xa0` = `ai.cfg`
  - `+0x78/+0xa4` = `cs.cfg`
  - `+0x7c/+0xa8` = `bl.cfg`
  - `+0x80/+0xac` = `il.cfg`
  - `+0x84/+0xb0` = `rl.cfg`
  - `+0x88/+0xb4` = `cl.cfg`
  - `+0x90/+0xb8` = `cui.cfg`
- active runtime materialization still splits across those exact mappings:
  - `hl/an/pi/ai/cs/rl/cl` are live with non-zero payloads on the current path
  - `bl` is live with a small non-zero payload
  - `il` currently reaches a live gate with `flag=1` but `ptr=null length=0`
  - `cui` currently remains absent on the active path (`flag=0 ptr=null length=0`)
- logs label these as:
  - `HasLiveCorpus..`
  - `GetLiveCorpus..`

## Exact isolated pairs now closed enough

Static proof now closes ten exact corpus pairs end-to-end:

1. `hl.cfg`
   - client helper `0x62198670`
   - mediator `+0x68`, then `+0x94`
   - original launcher:
     - `+0x68 -> 0x41f0c0 = owner byte +0x140e`
     - `+0x94 -> 0x41ad80 = owner ptr +0x1408`, out-length `+0x140c`
   - replacement state8 producer:
     - section selector `6`
     - owner `allocatedBuffer1408/140c/flag140e`

2. `an.cfg`
   - client helper `0x62198770`
   - mediator `+0x6c`, then `+0x98`
   - original launcher:
     - `+0x6c -> 0x41f0d0 = owner byte +0x1416`
     - `+0x98 -> 0x41ada0 = owner ptr +0x1410`, out-length `+0x1414`
   - replacement state8 producer:
     - section selector `7`
     - owner `allocatedBuffer1410/1414/flag1416`

3. `pi.cfg`
   - client helper `0x62198870`
   - mediator `+0x70`, then `+0x9c`
   - original launcher:
     - `+0x70 -> 0x41f0e0 = owner byte +0x141e`
     - `+0x9c -> 0x41adc0 = owner ptr +0x1418`, out-length `+0x141c`
   - replacement state8 producer:
     - section selector `3`
     - owner `allocatedBuffer1418/141c/flag141e`
   - newer client-side tightening:
     - `0x62198870 = LoadOrAdoptSelectionPiCfgAndMaybePersist`
     - live arg6 path calls `0x621c9d70 = AdoptLiveSelectionPiCfgCompactRecords`
     - that live adopter consumes compact **9-byte** tuples:
       - `u8 id + u32 value0 + u32 value1`
       - and writes only the first two dwords of each in-memory 12-byte slot at
         `DAT_629ea4e8 + 0x1d8 + id*0xc`
     - file fallback instead calls `0x621c9ce0 = LoadSelectionPiCfgIfPresent`
     - that file loader consumes on-disk **13-byte** records:
       - `u8 id + 12-byte slot body`
   - user-visible validation:
     - wiring this pair live moved the old missing Inventory/equipment-loadout symptom
   - newest replacement live comparison now shows the compact arg6 path is at least internally
     self-consistent across characters:
     - `Morg4n`: `HasLivePiCfg70/GetLivePiCfg9c` length `0x010e = 30 * 9`, and the client later logs
       exactly `30` non-zero `DAT_629ea4e8` entries
     - `Noobish`: `HasLivePiCfg70/GetLivePiCfg9c` length `0x002d = 5 * 9`, and the client later logs
       exactly `5` non-zero entries
     - practical consequence:
       - current replacement no longer looks like it is flattening `Morg4n` into the sparse
         `Noobish` live table on the launcher->client seam
       - the remaining fidelity question is narrower: whether the compact live bytes are still
         original-faithful, and/or how the client later consumes the richer `Morg4n` table into the
         render/fx path
     - newest replacement runtime observation also tightened the client-side slot shape without yet
       proving correctness:
       - at the first late client-shell transition, the active 30 ids were present with the expected
         `value0/value1` pairs
       - but every one of the full 107 in-memory 12-byte slots also retained the same third dword
         `0x627d7b00`
       - because `AdoptLiveSelectionPiCfgCompactRecords (0x621c9d70)` only writes the first two
         dwords, that common third-dword tail is inherited from prior object state rather than from
         launcher live bytes
       - this is now a concrete downstream fidelity question on the active path: whether that common
         retained tail is the original live compact-path default, or whether later render/fx code
         expects a different per-entry third field that only the alternate/full path materializes

4. `ai.cfg`
   - client helper `0x62198970`
   - mediator `+0x74`, then `+0xa0`
   - original launcher:
     - raw bytes at `0x41f0f0 = return owner byte +0x1426`
     - `+0xa0 -> 0x41ade0 = owner ptr +0x1420`, out-length `+0x1424`
   - replacement state8 producer:
     - section selector `4`
     - owner `allocatedBuffer1420/1424/flag1426`
   - symptom-relevance clue from the client consumer:
     - live adopter `0x621e2310` calls `0x621e0b90(...)`
     - then marks `param_1 + 0x6e0 + actionId*8 = 1`
     - current best narrow read: this pair is materially closer to action availability than the
       nearby path-builder chatter from `+0x38/+0x40`

5. `cs.cfg`
   - client helper `0x62198a70`
   - mediator `+0x78`, then `+0xa4`
   - original launcher:
     - raw bytes at `0x41f100 = return owner byte +0x142e`
     - `+0xa4 -> 0x41ae00 = owner ptr +0x1428`, out-length `+0x142c`
   - replacement state8 producer:
     - section selector `5`
     - owner `allocatedBuffer1428/142c/flag142e`
   - symptom-relevance clue from the client consumer:
     - live adopter `0x621cd550` writes entries into `param_1 + 0x6dc + index*8`
     - saved-file writer `0x621966d0 -> 0x621c9e20` later persists that table as 10-byte records
     - newer client-side tightening on the second dword of each 8-byte slot:
       - `0x621ca0c0` reads the low byte at `+0x6e0 + index*8`
       - `0x621e0a10` clears that low byte
       - `0x621e3b50` sets that low byte to `1` when the corresponding first dword slot is already non-zero
       - the only scaled-index **full-dword** writer currently isolated for that second slot dword is `0x621e1a70`
       - and both current callsites into `0x621e1a70` (`0x621c8ba0` and `0x621798d6`) pass a partially initialized 8-byte stack temp where:
         - dword0 is written intentionally
         - only the low byte of dword1 is explicitly zeroed
         - the upper 24 bits of dword1 are left as stack carry / garbage
       - current best read is therefore stronger than a generic “opaque metadata” label:
         - low bit/byte of dword1 is the meaningful availability flag family
         - upper 24 bits of saved dword1 are likely non-semantic stack carry from the client's temporary full-slot copy path
     - newer object-instance proof now removes the most tempting wrong branch:
       - `0x62198a70` loads live `cs.cfg` into **`0x629ea4e8`** before calling `0x621cd550`
       - `0x621966d0` uses **`0x629e95a8`** only for path/profile-root building through `0x62195ff0`
       - but the actual writer tail inside `0x621966d0` switches back to **`ecx = 0x629ea4e8`** before calling `0x621c9e20`
       - the low-byte setters `0x621e0a10`, `0x621e3b50`, and the current isolated `0x621e1a70` callsites also all run with **`ecx = 0x629ea4e8`**
       - so the meaningful saved-slot source is the same live table object, not a separate save-only slot table
     - important caution from the current `InitClientDLL_BeginLoadingCharacterFlow (0x62170b00)` path:
       - `ai.cfg` and `cs.cfg` are loaded through different stack objects there (`+0x84` vs `+0x94`)
       - so the close `+0x6dc/+0x6e0` structural resemblance is real, but it is not proof that both loaders are mutating the same object instance
     - newer save-order consequence from `PersistSelectionCfgCorpusIfDirty`:
       - the event-`0x0b` corpus walk calls `0x62198a70` (`cs.cfg`) **before** `0x62198970` (`ai.cfg`)
       - so the same-pass mediator-backed `ai.cfg -> 0x621e2310` load cannot be the thing that first creates the low-bit `1` values already written by that same `cs.cfg` save tail
       - current best narrowing is therefore: those low bytes must already be present on `0x629ea4e8` before `0x62198a70` reaches `0x621966d0 -> 0x621c9e20`

6. `rl.cfg`
   - client helper `0x62198d50`
   - mediator `+0x84`, then `+0xb0`
   - original launcher:
     - `+0x84 -> 0x41f130 = owner byte +0x1448`
     - `+0xb0 -> 0x41ae80 = owner ptr +0x1440`, out-length `+0x1444`
   - replacement state8 producer:
     - section selector `8`
     - owner `allocatedBuffer1440/1444/flag1448`

7. `cl.cfg`
   - client helper `0x62198e50`
   - mediator `+0x88`, then `+0xb4`
   - original launcher:
     - `+0x88 -> 0x41f140 = owner byte +0x1452`
     - `+0xb4 -> 0x41aea0 = owner ptr +0x144c`, out-length `+0x1450`
   - replacement state8 producer:
     - section selector `9`
     - owner `allocatedBuffer144c/1450/flag1452`

8. `bl.cfg`
   - client helper `0x62198b70`
   - mediator `+0x7c`, then `+0xa8`
   - original launcher:
     - raw bytes at `0x41f110 = return owner byte +0x13fe`
     - `+0xa8 -> 0x41ae40 = owner ptr +0x13f8`, out-length `+0x13fc`
   - replacement state8 producer:
     - section selector `1`
     - owner `allocatedBuffer13f8/13fc/flag13fe`
   - newer original live breakpoint proof now also closes the active-path shape here:
     - at real original `matrix.exe` stop `0x41f110`, owner `+0x13fe == 1`
     - owner `+0x13f8` was non-null
     - owner `+0x13fc/+0x13fe` read back as dword `0x0001000e`
       - i.e. length word `0x000e` (`14`), flag byte `0x01`
     - direct memory read of that original live payload at owner `+0x13f8` produced:
       - first dword `0x00006dd0`
       - trailing string bytes `"Nicodemus\0"`
   - active current-path runtime note:
     - replacement rerun logs now show the same small live payload length `0x000e` (`14`)
     - replacement payload preview now also matches that recovered original shape:
       - first dword `0x00006dd0`
       - trailing string `"Nicodemus"`

9. `il.cfg`
   - client helper `0x62198c60`
   - mediator `+0x80`, then `+0xac`
   - original launcher:
     - raw bytes at `0x41f120 = return owner byte +0x1406`
     - `+0xac -> 0x41ae60 = owner ptr +0x1400`, out-length `+0x1404`
   - replacement state8 producer:
     - section selector `2`
     - owner `allocatedBuffer1400/1404/flag1406`
   - newer original live breakpoint proof now closes the odd contract more tightly:
     - at real original `matrix.exe` stop `0x41f120`, owner `+0x1406 == 1`
     - at the same stop, owner `+0x1400 == 0`
     - and owner `+0x1404/+0x1406` read back as dword `0x00010000`
       - i.e. length word `0x0000`, flag byte `0x01`
     - practical consequence:
       - the seemingly strange replacement shape `HasLiveIlCfg80 -> 1` with `GetLiveIlCfgAc -> null`
         is actually faithful to the original active path
       - the earlier replacement change that suppressed the flag when section-2 bytes were zero was
         therefore a **fidelity regression** and has been reverted
       - current best read is that original client helper `0x62198c60` intentionally treats this as
         a distinct "live il.cfg path with empty payload/default object state" rather than as an
         ordinary file-fallback case

10. `cui.cfg`
   - client helper `0x621993d0`
   - mediator `+0x90`, then `+0xb8`
   - original launcher:
     - raw bytes at `0x41f160 = return owner byte +0x145a`
     - `+0xb8 -> 0x41ae20 = owner ptr +0x1454`, out-length `+0x1458`
   - replacement state8 producer:
     - section selector `10`
     - owner `allocatedBuffer1454/1458/flag145a`
   - active current-path runtime note:
     - rerun logs currently show this pair still absent on the active path (`flag=0 ptr=null length=0`)
   - newer writer-side tightening now makes the remaining mismatch narrower than a generic mapping gap:
     - `0x621993d0` checks arg6 `+0x90`
       - when false, it only tries to load an already-existing on-disk `cui.cfg`
       - when true, it can adopt live mediator data from `+0xb8`
     - direct writer `0x62197050` still exists separately and writes `cui.cfg` from client-owned object `0x629e05bc`
     - later bulk direct-save helper `0x62198490` can call `0x62197050` on shutdown-side save paths
     - practical consequence:
       - a later replacement `cui.cfg` file is not proof that state8 section `10` was live

That makes these exact mappings high-confidence:

- `arg6 +0x68/+0x94` = `hl.cfg`
- `arg6 +0x6c/+0x98` = `an.cfg`
- `arg6 +0x70/+0x9c` = `pi.cfg`
- `arg6 +0x74/+0xa0` = `ai.cfg`
- `arg6 +0x78/+0xa4` = `cs.cfg`
- `arg6 +0x7c/+0xa8` = `bl.cfg`
- `arg6 +0x80/+0xac` = `il.cfg`
- `arg6 +0x84/+0xb0` = `rl.cfg`
- `arg6 +0x88/+0xb4` = `cl.cfg`
- `arg6 +0x90/+0xb8` = `cui.cfg`
- replacement state8 sections `6`, `7`, `3`, `4`, `5`, `1`, `2`, `8`, `9`, and `10`

## Validation rerun after the first live pair

Representative rerun condition:
- command: `timeout 180 make run`
- active path: default replacement existing-character path
- no broad auth/bootstrap/state9 changes in the same pass

Observed validation status in `~/MxO_7.6005/resurrections_spdlog.log` now splits into four useful parts:

Previously observed live pairs:
- `MediatorStub::HasLiveCorpus68(+0x68) -> 1`
- `MediatorStub::GetLiveCorpus94(+0x94) -> ... [length=0x0316]`
- `0x0316 = 790`, matching the observed original `hl.cfg` size
- `MediatorStub::HasLiveCorpus6C(+0x6c) -> 1`
- `MediatorStub::GetLiveCorpus98(+0x98) -> ... [length=0x0042]`
- `0x0042 = 66`, matching the observed original `an.cfg` size
- `MediatorStub::HasLiveCorpus74(+0x74) -> 1`
- `MediatorStub::GetLiveCorpusA0(+0xa0) -> ... [length=0x001e]`
- `0x001e = 30`, matching the observed original `ai.cfg` size

Newer now-confirmed live pairs:
- `MediatorStub::HasLiveCorpus70(+0x70) -> 1`
- `MediatorStub::GetLiveCorpus9C(+0x9c) -> ... [length=0x010e]`
- `0x010e = 270`, matching the observed original `pi.cfg` size
- `MediatorStub::HasLiveCorpus84(+0x84) -> 1`
- `MediatorStub::GetLiveCorpusB0(+0xb0) -> ... [length=0x06a8]`
- `0x06a8 = 1704`, matching the observed original `rl.cfg` size
- `MediatorStub::HasLiveCorpus88(+0x88) -> 1`
- `MediatorStub::GetLiveCorpusB4(+0xb4) -> ... [length=0x0132]`
- `0x0132 = 306`, matching the observed original `cl.cfg` size

Newly mapped/implemented remaining pairs on the active rerun:
- `MediatorStub::HasLiveCorpus7C(+0x7c) -> 1`
- `MediatorStub::GetLiveCorpusA8(+0xa8) -> ... [length=0x000e]`
- `0x000e = 14`, current active-path `bl.cfg` payload size
- `MediatorStub::HasLiveCorpus80(+0x80) -> 1`
- `MediatorStub::GetLiveCorpusAC(+0xac) -> null [length=0x0000]`
- active current-path read: exact `il.cfg` pair is mapped and reached, but the current reply materialization is an empty payload
- `MediatorStub::HasLiveCorpus90(+0x90) -> 0`
- active current-path read: exact `cui.cfg` pair is mapped, but no current section-10 payload is present on this path
- newer bounded rerun after the narrow `RunClientDLL -> TermClientDLL` follow-up proves a second, later distinction:
  - replacement live logs still show `HasLiveCorpus90(+0x90) -> 0`
  - but deleting `~/MxO_7.6005/Profiles/morgan/Reality_3ED980/cui.cfg` and rerunning still recreates that file
    - recreated replacement file:
      - size `165`
      - SHA-256 `cd5b1fd7266222b582f563118b81db03afdfaab77dcfeacedee34ddf35a37226`
  - a fresh bounded original rerun still updates the normal saved corpus (`ai/an/cl/cs/hl/mcd/pi/rl`) but still leaves `cui.cfg` absent in
    `~/.wine/drive_c/users/morgan/AppData/Local/The Matrix Online/Profiles/morgan/Morg4n_6DCE/`
  - practical consequence:
    - current replacement/original `cui.cfg` divergence is now narrowed to a later client-owned save-path condition,
      not to the already-closed `+0x90/+0xb8` pair mapping itself

New exact clarification on the old `cs.cfg` size mismatch:
- `client.dll:0x62198a70` live-load path calls `0x621cd550`
- `0x621cd550` consumes **6-byte** compact records and writes only the first dword of an 8-byte in-memory slot
- `client.dll:0x621966d0 -> 0x621c9e20` later saves **10-byte** on-disk records (`u16 index + 8-byte slot`) for non-zero entries
- current replacement counts now line up exactly with that format split:
  - live state8 section `5` / mediator `+0x78/+0xa4` = `2706 = 451 * 6`
  - saved `cs.cfg` on disk = `4510 = 451 * 10`
- current original vs replacement on-disk comparison after rerun tightens the split further:
  - all `451` record ids match
  - the first dword of all `451` saved records matches bit-for-bit
  - the second dword differs across all `451` saved records
- tighter second-dword evidence from the original reference corpus:
  - only `5` original saved `cs.cfg` records carry low-bit `1`
  - those exact ids are `{0, 1, 7, 15, 19}`
  - the original `ai.cfg` compact records also target exactly `{0, 1, 7, 15, 19}`
  - current best read: the low bit/byte of saved `cs.cfg` slot dword2 is the same broader availability state family surfaced by the `ai.cfg` path
- newer object-instance/save-order proof tightens the remaining question further:
  - the outer `0x621966d0` helper does touch `0x629e95a8`, but only for path/profile-root building through `0x62195ff0`
  - the actual `cs.cfg` writer tail is `0x6219679d: mov ecx,0x629ea4e8 ; call 0x621c9e20`
  - the live `cs.cfg` adopter is likewise `0x62198aaa: mov ecx,0x629ea4e8 ; call 0x621cd550`
  - the low-byte setters and current isolated full-slot writer also use that same live table object:
    - `0x62179c47/0x62179c9e -> ecx=0x629ea4e8 -> 0x621e3b50/0x621e0a10`
    - `0x621798d1` and `0x621c8be1 -> ecx=0x629ea4e8 -> 0x621e1a70`
  - practical consequence: the replacement/original low-bit gap is **not** explained by `0x621c9e20` saving from the wrong object instance
  - additional save-order consequence from `PersistSelectionCfgCorpusIfDirty`:
    - `0x62198a70` (`cs.cfg`) runs before `0x62198970` (`ai.cfg`)
    - so the same event-`0x0b` save walk cannot first get its low-bit `1` values from that later mediator-backed `ai.cfg` load
    - the remaining exact question is therefore which **earlier** live mutation of `0x629ea4e8` natural-original reaches before `0x62198a70 -> 0x621966d0 -> 0x621c9e20`
- bounded original-runtime breakpoint pass now tightens that timing claim further:
  - binary / workflow:
    - real original launcher path
    - attach to spawned temp `matrix.exe` with WineDbg at a stable pre-`InitClientDLL` UI point
    - use Wine PID attach discipline from `WINEDBG.md`
  - armed breakpoints:
    - `matrix:0x40a4d0`
    - `client:0x620012a0`
    - `0x62198a70`
    - `0x621966d0`
    - `0x62198970`
    - `0x621e3b50`
    - `0x621e0a10`
    - `0x621e1a70`
  - observed stop order on that pass:
    - `0x62198a70`
    - `0x621966d0`
    - `0x62198970`
    - first visible `0x621e3b50`
  - later breakpoint-hit counts on the same attached run also show:
    - `0x621e0a10` hit `1`
    - `0x621e1a70` hit `1`
  - practical consequence:
    - on the natural original pass, the watched `0x629ea4e8` mutators did **not** precede the `cs.cfg` save pair
      `0x62198a70 -> 0x621966d0`
    - so the remaining live-source question is now earlier/narrower than those specific watched setter hits
  - extra first-hit note from the visible `0x621e3b50` stop:
    - `ecx = 0x629ea4e8`
    - the observed slot id read from `*(ushort*)(*(int*)(param_2+0x10)+6)` was `0x208`
    - so that first post-save setter hit was a sentinel/out-of-range path, not immediate proof of one of the five `{0,1,7,15,19}` ids
  - important exit-path clarification from the same live original debugging pass:
    - on soft exit / shutdown, the debugger later hit `0x621966d0` again
    - that hit came from `client.dll:0x620011a0 = TermClientDLL` through `0x6216a2f0 -> 0x62198490 = PersistSelectionCfgCorpusFromEnableFlags`
    - that shutdown helper calls `0x621966d0` **directly**, without first going through `0x62198a70` or `0x62198970`
    - practical consequence:
      - an exit-time `0x621966d0` hit is a real save, but it is **not** the same pre-save/load-order proof surface as the earlier in-run `0x62198a70 -> 0x621966d0 -> 0x62198970` sequence
      - do not mix that shutdown direct-save path into the narrower question of what pre-populates `0x629ea4e8` before the in-run `cs.cfg` save boundary
  - strongest current runtime proof from the corrected original pass:
    - stop at `0x62198ab4` (inside `0x62198a70`, after `0x621cd550`, before the same-pass `0x621966d0` save tail)
      - ids `{0,1,7,15,19}` all showed `slot dword2 = 0x627d7b00`
      - nearby control ids `{2,3,4,5,6}` also remained `0x627d7b00`
      - practical read: the in-run `cs.cfg` save boundary still has low-bit `0` for the exact five interesting ids
    - stop at `0x621989b4` (inside `0x62198970`, after `0x621e2310`, before `ai.cfg` save)
      - ids `{0,1,7,15,19}` all showed `slot dword2 = 0x627d7b01`
      - nearby control ids `{2,3,4,5,6}` remained `0x627d7b00`
      - practical read: the mediator-backed/live `ai.cfg` load is exactly what flips the low bit on those five `cs` slots in memory
    - later shutdown stop at direct `0x621966d0` from `TermClientDLL`
      - the same five ids `{0,1,7,15,19}` still showed `0x627d7b01`
      - nearby control ids still remained `0x627d7b00`
      - practical read: the later direct/shutdown `cs.cfg` save path can naturally persist the same exact five low-bit `1` values seen in the original reference file
  - current best explanation of the original vs replacement low-bit gap is therefore no longer vague:
    - the early in-run `0x62198a70 -> 0x621966d0` `cs.cfg` save happens **before** the live `ai.cfg` load flips those five low bits
    - the original reference `cs.cfg` with low-bit `1` for `{0,1,7,15,19}` is best explained by a **later** `0x621966d0` save after `0x62198970` has already mutated the shared `0x629ea4e8` table
    - the clean bounded example of that later save is the direct shutdown path from `TermClientDLL`
  - newer caller-surface tightening keeps that bounded shutdown read stronger than the broader static caller list alone:
    - static xrefs show `0x621966d0` can also be reached through:
      - `0x62198490 = PersistSelectionCfgCorpusFromEnableFlags`
      - `0x62197db0` per-id save helper
    - and `0x62198490` itself is reachable from:
      - `0x621707e0 = ClientShell_LoginMediatorObserver_OnEvent` event-side handler
      - `0x62171600 = ClientShell_RunLaterRuntimeTransitionHelper` later runtime helper
      - `0x6216a2f0` / `TermClientDLL`
    - but a fresh bounded original runtime pass on spawned `matrix.exe` now narrows the **active current-route** behavior further:
      - observed event sequence before entry/later tail was:
        - `0x10`
        - `0x0e`
        - `0x11`
        - `0x09`
        - `0x12`
        - `0x17`
        - `0x0b`
        - `0x18`
        - `0x0f`
      - on that pass we did **not** stop on `ClientShell_RunLaterRuntimeTransitionHelper (0x62171600)` or `0x62197db0` before entering game / before the later exit
      - the next later direct-save-family hit after the in-run `0x62198a70 -> 0x621966d0 -> 0x62198970` sequence was instead:
        - `0x62198490`
        - with backtrace showing `0x620011aa` / `TermClientDLL`
    - practical consequence:
      - keep `ClientShell_LoginMediatorObserver_OnEvent (0x621707e0)` /
        `ClientShell_RunLaterRuntimeTransitionHelper (0x62171600)` / `0x62197db0` as real static later-save candidates globally
      - but for the currently bounded active original route, shutdown `TermClientDLL -> 0x62198490 -> 0x621966d0` is still the strongest concrete later-save explanation for the saved `{0,1,7,15,19}` low-bit pattern
  - replacement-side comparison is now concrete enough to close the exact late-save question without reopening broad launcher work:
    - narrow source change in `src/resurrections.cpp` now follows positive `RunClientDLL` with `TermClientDLL`
    - bounded rerun condition:
      - `timeout 300 make run`
    - rerun logs still show the active path reaching event `0x0b` and later `RunClientDLL returned: 1`
    - after that return, the same rerun immediately shows later save-family getter traffic again
      (`GetSelectionContextSnapshot(+0xf4)` from the `mcd.cfg` persistence family), which is consistent with entering shutdown-side client persistence under `TermClientDLL`
    - current narrow caveat from that rerun:
      - the replacement process exited before the launcher logged `TermClientDLL returned: ...`
      - so do **not** overclaim a clean launcher-side post-`TermClientDLL` return yet from this pass alone
    - but the file result closes the selection-cfg question itself:
      - replacement save:
        - `~/MxO_7.6005/Profiles/morgan/Reality_3ED980/cs.cfg`
      - original reference:
        - `~/.wine/drive_c/users/morgan/AppData/Local/The Matrix Online/Profiles/morgan/Morg4n_6DCE/cs.cfg`
      - both files are `4510` bytes
      - both files are bit-identical
      - shared SHA-256:
        - `5575bdd5673f8fa2eeea418748c829910661d3132cdd9c9e5f8c17ea6dd75047`
      - the saved low-bit pattern is now exactly `{0,1,7,15,19}`
    - practical consequence:
      - for the active bounded replacement route, the remaining `cs.cfg` low-bit gap is solved by the narrow faithful `RunClientDLL -> TermClientDLL` sequencing change
      - explicit deferral is no longer the better explanation for this exact question
- newer writer-path tightening still explains why the saved second dword should not be treated as fully semantic authored metadata:
  - `0x621e1a70` is the only current scaled-index full-dword writer for slot dword2
  - and both current callers feed it a temp whose upper 24 bits are never initialized intentionally
  - current best read remains that those upper bits are non-semantic stack carry, even though the bounded shutdown-parity rerun no longer needs that theory to excuse a replacement/original mismatch
- practical consequence:
  - the shorter live `section 5` size is no longer evidence by itself that the replacement is missing `cs.cfg` entries; it is a different compact wire/runtime format than the later persisted file
  - on the active bounded replacement route, the saved-file `cs.cfg` low-bit parity question is now closed by the later shutdown/direct-save path

Current outcome:
- user-visible validation history across these passes now splits usefully:
  - before `ai.cfg`, Actions (`O`) was still missing Hyper-Jump / Hyper-Speed / Hyper-Sprint
  - after `ai.cfg`, those Actions reappeared
  - after `pi.cfg`, Inventory/equipment loadout now works on the active path
- bounded save-side parity for the remaining `cs.cfg` question is now also closed enough on the active replacement route:
  - the narrow `RunClientDLL -> TermClientDLL` follow-up now produces a replacement `cs.cfg` bit-identical to the refreshed original reference file
- `rl.cfg` and `cl.cfg` are now also live end-to-end from state8-backed mediator storage on rerun,
  so the old broader equipment/loadout fallback theory no longer needs those two pairs kept in the
  speculative bucket

## Why this family is now the best next suspect

`mcd.cfg` now writes correctly and matches the original bit-for-bit, but the replacement still shows
runtime parity gaps that fit missing non-`mcd.cfg` corpus state much better than missing auth or broad
state9 work.

The old symptom cluster has now narrowed materially:
- Hyper-Jump / Hyper-Speed / Hyper-Sprint actions reappeared after `ai.cfg`
- Inventory/equipment loadout now works after `pi.cfg`
- the main remaining user-visible question is whether the still-short `cs.cfg` replacement data is the last meaningful blocker for any lingering skills / bonus-skills parity

That keeps this pre-`mcd.cfg` live-corpus family as the highest-value remaining exact RE surface without reopening broad auth/bootstrap/state9 work.

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

Notably absent there on the current original reference character:
- `bl.cfg`
- `il.cfg`
- `cui.cfg`

## Current replacement state8 section-size correlations

The replacement already materializes these state8 reply sections on the active path:

- section `1`  = `14`
- section `2`  = `0`
- section `3`  = `270`
- section `4`  = `30`
- section `5`  = `2706`
- section `6`  = `790`
- section `7`  = `66`
- section `8`  = `1704`
- section `9`  = `306`
- section `10` = absent on the current active rerun

Current working split:
- exact current closure from static client + launcher + source-owned state8 evidence:
  - section `1`  ↔ `bl.cfg`
  - owner `+0x13f8/+0x13fc/+0x13fe`
  - arg6 `+0x7c/+0xa8`
  - section `2`  ↔ `il.cfg`
  - owner `+0x1400/+0x1404/+0x1406`
  - arg6 `+0x80/+0xac`
  - section `3`  ↔ `pi.cfg`
  - owner `+0x1418/+0x141c/+0x141e`
  - arg6 `+0x70/+0x9c`
  - section `4`  ↔ `ai.cfg`
  - owner `+0x1420/+0x1424/+0x1426`
  - arg6 `+0x74/+0xa0`
  - section `5`  ↔ `cs.cfg`
  - owner `+0x1428/+0x142c/+0x142e`
  - arg6 `+0x78/+0xa4`
  - section `6`  ↔ `hl.cfg`
  - owner `+0x1408/+0x140c/+0x140e`
  - arg6 `+0x68/+0x94`
  - section `7`  ↔ `an.cfg`
  - owner `+0x1410/+0x1414/+0x1416`
  - arg6 `+0x6c/+0x98`
  - section `8`  ↔ `rl.cfg`
  - owner `+0x1440/+0x1444/+0x1448`
  - arg6 `+0x84/+0xb0`
  - section `9`  ↔ `cl.cfg`
  - owner `+0x144c/+0x1450/+0x1452`
  - arg6 `+0x88/+0xb4`
  - section `10` ↔ `cui.cfg`
  - owner `+0x1454/+0x1458/+0x145a`
  - arg6 `+0x90/+0xb8`

Current active runtime split inside those exact mappings:
- `bl.cfg` reaches a live non-zero payload
- `il.cfg` is currently an empty payload
- `cui.cfg` is currently absent on the active path
- the rest above reach the non-zero payloads already confirmed by rerun logs

Important exact correction on the similarly-named `btl.cfg` sibling:
- `btl.cfg` is **not** one of the state8 live mediator sections above
- client static proof now splits it cleanly:
  - `client.dll:0x62197230 = LoadSelectionBtlCfgIfPresent`
    - only **loads an already-existing** `btl.cfg` during `PersistSelectionCfgCorpusIfDirty`
  - `client.dll:0x62196b80 = PersistSelectionBtlCfg`
    - is the dedicated **writer** used only by the later bulk save helper
      `0x62198490 = PersistSelectionCfgCorpusFromEnableFlags`
- refreshed active reference check now leaves the remaining neighboring file diffs as:
  - original-only `btl.cfg`
  - saved `cs.cfg` five-byte low-bit family `{0,1,7,15,19}`
  - while `mcd.cfg` has returned to bit-identical parity on the active route
- practical consequence:
  - missing replacement `btl.cfg` on a crash/early-stop route is a later direct-save reach/order
    gap, not evidence that the current state8 section mappings are wrong

## Active source ownership after cleanup split

Focused replacement source files:
- `src/launcher_mediator_abi_selection_cfg.cpp`
  - non-`mcd.cfg` corpus wrapper surface `+0x68 .. +0xb8`
  - current cleanup status:
    - the full non-`mcd.cfg` corpus family `+0x68 .. +0xb8`
      now thin-forwards into named `CLTLoginMediator` accessors
    - the `cui.cfg` caveat stays explicit on the class-owned `+0x90/+0xb8` pair:
      live mediator data is still absent on the active replacement path even though later client-owned shutdown persistence may still emit on-disk `cui.cfg`
- `src/launcher_mediator_abi_profile_paths.cpp`
  - neighboring profile-path / current-slot / selection-descriptor surface `+0x3c/+0x40/+0x44`
- `src/launcher_mediator_abi_mcd.cpp`
  - already-closed `mcd.cfg` surface
- `matrixstaging/game/src/libltclientlogin/loginstate_state8.cpp`
  - real producer-side state8 section materialization
- `matrixstaging/game/src/libltclientlogin/loginmediator.h`
  - owner-side raw state8/load-character storage layout
  - named class-owned live-corpus accessors for the migrated `hl/an/pi/ai/cs/bl/il` pairs

Implementation note:
- the focused ABI files are currently split as separate source-owned implementation files included by
  the broader `src/launcher_mediator_abi.cpp` shell, so future RE passes can stay on one surface
  without rereading the whole ABI file

## Practical next step

Current exact mapping set is now source-owned for the full non-`mcd.cfg` helper family.
That changes the next step from “which pair is this?” to “which remaining runtime detail still matters?”

Recommended next pass:
1. keep the current exact live mappings in place for:
   - `hl/an/pi/ai/cs/bl/il/rl/cl/cui`
2. treat the old `cs.cfg` size mismatch as a **format** question, not a missing-entry-count question
   - live section `5` is now proven compact `6`-byte records
   - saved `cs.cfg` is the later expanded `10`-byte per-entry form
3. keep the remaining on-disk parity questions split by the exact client save family that owns them:
   - `cs.cfg` final low-bit parity and `btl.cfg` creation both belong to the later bulk-save path
     rooted at `0x62198490 = PersistSelectionCfgCorpusFromEnableFlags`
   - do **not** try to "fix" those by inventing new mediator/state8 payloads when the real missing
     thing may only be later reach/order into the client-owned bulk save writer family
4. the next highest-value remaining exact question inside this corpus is now the narrowed `cui.cfg` save mismatch:
   - live mediator pair `+0x90/+0xb8` is still absent on the active replacement path
   - but the later replacement shutdown/direct-save family still recreates on-disk `cui.cfg`
   - while the bounded original route still omits it
   - focus next on which later client-owned save-path condition preserves or clears the dirty/save gate before `0x62198490 -> 0x62197050`
5. after that, if any skills / bonus-skills parity is still missing, focus next on:
   - what the saved-`cs.cfg` second dword really means on the client side
   - whether the current `il.cfg` empty payload is faithful on the active path

Do not reopen broad auth/bootstrap or generic state9 history unless this exact corpus family forces it.
