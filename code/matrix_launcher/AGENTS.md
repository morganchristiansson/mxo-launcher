# Matrix Online Launcher - Agent Development Notes

For generic workflow and documentation policy, see:
- `../../AGENTS.md`

## Current Goal

Reimplement the original Matrix Online `launcher.exe` startup path as faithfully as possible.

Source of truth:
- `~/MxO_7.6005/launcher.exe`

Current active sources:
- `src/resurrections.cpp`
- `src/diagnostics.cpp`
- `src/diagnostics.h`
- `src/liblttcp/ltthreadperclienttcpengine.h`
- `src/liblttcp/ltthreadperclienttcpengine.cpp`
- `src/liblttcp/lttcpconnection.h`
- `src/liblttcp/lttcpconnection.cpp`
- `src/liblttcp/cmessageconnection.h`
- `src/liblttcp/cmessageconnection.cpp`

## Current Status (2026-03-12)

### Faithful path now implemented in the scaffold
- preloads support DLLs
- loads `cres.dll` before `client.dll`
- resolves:
  - `InitClientDLL`
  - `RunClientDLL`
  - `TermClientDLL`
  - `ErrorClientDLL`
- uses the correct original **8-argument** `InitClientDLL` frame shape
- strips launcher-only auth argv back out before `InitClientDLL`
- models arg7 the same packed way as original launcher from `CLauncher+0xa8/+0xac`
- refuses incomplete `InitClientDLL` by default

### Current blocker
The launcher still does **not** reconstruct enough launcher-owned startup state.
Current unresolved inputs:
- arg5: launcher object at `0x4d6304`
- arg6: `ILTLoginMediator.Default` from `0x4d2c58`
- arg7: packed selection state from `CLauncher+0xa8/+0xac`
- arg8: flag byte from `0x4d2c69`
- pre-client environment setup at `0x402ec0`

Current faithful experiment result:
- with original `client.dll` restored, we fall back to the old import/wrapper problem (`mxowrap.dll`) and do **not** keep the deeper progress path alive yet
- faithful launcher-owned startup state is still incomplete regardless

Current practical runtime note:
- active progress runs now intentionally use the hex-edited `client.dll` import variant (`dbghelp.dll` instead of `mxowrap.dll`)
- current file layout in `~/MxO_7.6005/`:
  - `client.dll` = active patched runtime binary
  - `client.dll.original` = original import layout backup
  - `client.dll.patched` = patched backup copy
- treat this as a pragmatic progress choice, not final faithful equivalence

Current deep diagnostic progress with patched client:
- under the `/home/morgan` user with working video/audio permissions, the game creates a real `MATRIX_ONLINE` window and reaches much deeper client startup
- current observed mediator surface on that path includes:
  - `+0x48`
  - `+0x4c`
  - `+0x170`
  - `+0x124`
- current logged sequence still stably reaches:
  - `GetWorldOrSelectionName()`
  - `GetProfileOrSessionName()`
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(netShell, netMgr, distrObjExecutive)`
  - `AttachStartupContext(second)`

Current practical crash state:
- current patched-client scaffold no longer dies on a simple missing-vtable-slot `EIP=0` case
- newer debugger + static narrowing identified a concrete stack-cleanup mismatch in the early mediator auth-name chain:
  - `client.dll:0x62001325..0x62001362` calls mediator `+0x58`, then passes that result to `+0x60`, then passes that result to `+0x5c`, then calls a formatter and finally does `add esp, 0x14`
  - that caller cleanup means mediator `+0x60` and `+0x5c` are expected to be **caller-clean single-arg methods**, not callee-clean `ret 4` methods
  - the scaffold had exposed both as normal `__thiscall` methods, so our replacement launcher was incorrectly popping 8 bytes too many before `client.dll:0x62001365`
- newer debugger-assisted proof of the overwrite mechanism:
  - at `client.dll:0x62001319`, the top-level `InitClientDLL` saved return address at `[ebp+4]` is still launcher `0x411187`
  - by `client.dll:0x62001365`, `esp` has drifted to `ebp+8`
  - so the next `push esi` writes current stale `esi = arg2 filteredArgv + 0x0c` directly over the saved return address slot
  - later returns then land in current `arg2` storage and produce the familiar late crash family
- the diagnostic mediator now exposes `+0x60` / `+0x5c` through naked caller-clean wrappers (`ret`, not `ret 4`)
- current highest-value rerun after that fix:
  - `MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Vector make run_binder_both`
  - the client still reaches the deeper mediator path through `ConsumeSelectionContext(...)` at `+0xec`
  - but the old late `EIP=arg2+2` crash family no longer reproduces on that run
  - `InitClientDLL` now returns cleanly to the launcher with value **`1`** instead of crashing
  - newer launcher-side handling now also treats this positive return as success rather than generically treating every non-zero return as failure
  - current `resurrections.log` now ends with:
    - `InitClientDLL returned: 1`
    - `InitClientDLL succeeded, but RunClientDLL is gated.`
- new deliberate `RunClientDLL` experiment on that same clean binder path:
  - `MXO_TRACE_WINDOWS=1 MXO_FORCE_RUNCLIENT=1 MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Vector make run_binder_both`
  - original launcher static review now confirms `0x40a5a9`, `0x40a624`, and `0x40a6be` all use `test eax,eax ; jg ...`, so positive `InitClientDLL` / `RunClientDLL` / `TermClientDLL` returns are the original success contract
  - the deliberate run now does **not** reproduce the old forced-runtime crash at `client.dll+0x3b3573`
  - no fresh crash dump was produced during timed runs
  - window tracing shows a real `MATRIX_ONLINE` window first appears centered, then flips to fullscreen `800x600`
  - the current stable `RunClientDLL` loop repeatedly hits:
    - mediator `+0x2c` (`IsConnected()` in the scaffold)
    - arg5 helper `+0x60` slot `0` / slot `1` (`EnterCriticalSection` / `LeaveCriticalSection`)
  - newer throttled queue-state logging now also shows the exact arg5 cursor state observed by that loop:
    - queue0C `current0 == current1 == block0 == block1`
    - queue34 `current0 == current1 == block0 == block1`
    - that remained unchanged through sampled counts `1 .. 1024`
  - newer static comparison now shows this is not just a loose resemblance:
    - client `0x62532130 -> 0x62531c10(1)` is the **non-blocking poll variant** of the same shared arg5 queue-consumer family seen in original launcher `0x436b10`
    - original launcher producer helper `0x436820 -> 0x436670` acquires `+0x60`, enqueues an 8-byte pair into queue0C/queue34, releases `+0x60`, and signals `+0x5c` when transitioning from empty to non-empty
    - in the currently identified concrete producer xrefs (`0x4302d5`, `0x4325aa`, `0x4329cc`, `0x449d8a`), the third arg to `0x436820` is always `0`, so present startup/runtime producer evidence is currently specific to **queue0C**
    - those xrefs also narrow the queued pair shape to:
      - first dword = small freshly allocated work-item-like object (`0x2c`/`0x20`/`0x0c` families via `0x435090` / `0x435010` / `0x435050` / `0x435070`)
      - second dword = stable owner/context pointer or associated object (for example `[esi+0x38]`, `edi`, or similar caller-held context)
    - the remaining concrete producer xrefs now read so far fall into several queue0C families:
      - existing-object + context submissions
      - fresh `0x0c` / `0x20` / `0x2c` work-item submissions
      - back-to-back multi-submit paths
      - null/fallback submissions
      - one looped submit-and-wait style path at `0x449d8a`
    - all currently identified concrete `0x436820` producer xrefs have now been read/documented:
      - `0x4302d5`
      - `0x43051f`
      - `0x43067f`
      - `0x4306a7`
      - `0x4309da`
      - `0x4309ef`
      - `0x430c25`
      - `0x430d71`
      - `0x430d94`
      - `0x430da8`
      - `0x4315b0`
      - `0x4325aa`
      - `0x4329cc`
      - `0x432d86`
      - `0x432dc1`
      - `0x432dd7`
      - internal self-calls: `0x436a0e`, `0x436fa8`
      - looped submit-and-wait path: `0x449d8a`
    - newer import-backed + string-backed narrowing now makes the producer family more concrete than “generic queued work”:
      - `0x4302d5` sits on a later `recvfrom` path, so it now looks like receive/packet-side event submission
      - helper `0x449b40` is a socket factory around `socket(AF_INET, type, protocol)` plus option setup
      - arg5 slot `1` / `0x431ce0` is now string-backed as `MonitorPort`
      - arg5 slot `2` / `0x4325d0` is now string-backed as `UDPMonitorPort` and uses `SOCK_DGRAM` / `IPPROTO_UDP`, then `setsockopt(...SO_REUSEADDR...)`, then `bind`
      - arg5 slot `6` / `0x4328a0` is now string-backed as `Connect` and uses `SOCK_STREAM` / `IPPROTO_TCP`, then `bind` + `connect`
      - arg5 slot `7` / `0x42f970` is now string-backed as `Close`
      - arg5 slot `8` / `0x42fbd0` is now string-backed as `SendBuffer`
      - arg5 slot `12` / `0x4316a0` is now string-backed as `CleanupConnection`
      - coded `0x435050(payload)` items now look like network status/result objects, not generic integer blobs
    - current best reading is therefore that queue0C is a launcher-owned **network-engine event/status queue**, not a generic task list
    - newer direct-xref review also narrows how the engine seems to be entered from startup:
      - current direct uses of global `0x4d6304` still only show construction/registration, `InitClientDLL` handoff, descriptor embedding/default fallback, and teardown release
      - there is still no obvious single direct launcher-mainline call on raw global `0x4d6304` to `MonitorPort` / `UDPMonitorPort` / `Connect`
      - newer worker/connection review now also shows why that may be so:
        - a likely `CMessageConnection`-family object captures the engine pointer at `+0x10`
        - then calls back into engine methods and also enqueues `(work, self, 0)` through `0x436820`
      - so the next missing startup activity is currently more likely to be indirect engine/helper/connection setup than one trivial direct global call we just missed
    - newer container/dispatch narrowing now also shows:
      - arg5 `+0x80` is an endpoint-keyed container (sockaddr/port-style key)
      - arg5 `+0x8c` is a pointer-keyed container (context/owner dword key)
      - `+0x80` payloads are now best read as `AcceptThread`-style worker objects populated by slot `1` / `MonitorPort`
      - `+0x8c` payloads are now best read as `WorkerThread`-style worker objects populated by helper `0x431ff0` for slot `2` / `UDPMonitorPort` and slot `6` / `Connect`
      - slot `2` success marks worker state `[worker+0x34] = 2`
      - slot `6` success marks worker state `[worker+0x34] = 1`
      - slot `7` / `Close` and slot `8` / `SendBuffer` both explicitly gate on connection state `1` or `2`
      - arg5 slot `5` (`0x431840`) is now best read as an endpoint-keyed unmonitor / teardown / handle-extraction path with miss result `0x7000004`
      - arg5 slot `12` (`0x4316a0` / `CleanupConnection`) acquires helper `+0x98`, looks up the dequeued context in `+0x8c`, consumes/removes a payload object there, performs teardown/state-transition work, and then calls `0x44ab60(context)`
      - launcher consumer `0x436d31..0x436ee7` dequeues `(workItem, context)`, reads `[workItem+0x04]` through `0x4816f0`, calls arg5 slot `12(context)`, then calls `context->+0x10(workItem)`
      - that dequeued `context` is now likely a `CMessageConnection`-family object on important paths, not just an anonymous owner pointer
      - current best `CMessageConnection` callback mapping is now:
        - `+0x10` / `0x4490c0` = likely `OnOperationCompleted(workItem)`
        - `+0x20` / `0x449d20` = likely `SendPacket(...)` -> engine `+0x20` / `SendBuffer`
        - `+0x1c` / `0x449cd0` = likely endpoint-update / ensure-connected wrapper -> engine `+0x18` / `Connect`
        - `+0x0c` / `0x449ca0` = likely close/abort wrapper -> engine `+0x1c` / `Close`
      - newer ctor/vtable review now also shows `0x448b40` first builds a base `CLTTCPConnection`-family object (vtable `0x4b8018`, string-backed by nearby `CLTTCPConnection::OnReceive()` text) and then overwrites the vtable to `0x4b7928` for `CMessageConnection`
      - newer wrapper-level signature narrowing now also shows those engine callbacks are importantly connection-object-based on this path:
        - `0x449cd0` updates connection `+0x24` and then calls engine `+0x18(self)`
        - `0x449ca0` calls engine `+0x1c(self, ...)`
        - `0x449d20` forwards send args together with `self` into engine `+0x20`
      - current best reading is therefore that the engine is often reached indirectly through `CMessageConnection`-family objects after they capture the engine pointer, not only through obvious direct raw-global `0x4d6304` calls
    - when queue work is present, client `0x62531e31..0x62531fe7` mirrors launcher `0x436d31..0x436ee7` and then calls arg5 primary vtable offset `+0x30` (slot `12`)
    - so the current absence of arg5 slot-12 runtime traffic is now best explained by **empty queue state**, not by slot 12 being irrelevant
  - current best reading is that the binder/scaffold path now reaches a real runtime idle/poll loop rather than an immediate post-init crash, and the next blocker is missing launcher-owned work/state that should drive arg5 queue activity beyond the current empty-loop path
  - newer startup-side owner-path review now makes that indirect activation path more concrete than a generic suspicion:
    - `0x41d170` constructs a `CMessageConnection`-family object, stores it at owner `+0x18`, builds endpoint state into owner `+0x5c`, then immediately calls `connection->+0x1c(owner+0x5c)`
    - `0x41e500` constructs another `CMessageConnection`-family object, stores it at owner `+0x1c`, builds endpoint state into owner `+0x6c`, then immediately calls `connection->+0x1c(owner+0x6c)`
    - current best mapping still reads that virtual `+0x1c` as the connection-oriented ensure-connected / engine-`Connect` wrapper
    - current xrefs for those owner methods are now concrete next targets:
      - `0x43909f -> 0x41d170`
      - `0x439345 / 0x43936b / 0x43938e / 0x4393bf -> 0x41e500`
      - with the latter set hanging off higher-level owner state rooted at `0x4f78b8`
    - newer server-config string recovery now also gives concrete auth/margin config anchors in both binaries:
      - launcher.exe:
        - `qsAuthServerDNSName`
        - `AuthServerPort` (default `0x2af8 = 11000`)
        - `MarginServerDNSSuffix`
        - `MarginServerPort` (default `0x2710 = 10000`)
      - client.dll:
        - `AuthServerDNSName`
        - `AuthServerPort` (default `0x2af8 = 11000`)
        - `MarginServerDNSSuffix`
        - `MarginServerPort` (default `0x2710 = 10000`)
      - important nuance: current recovered margin-side string is `MarginServerDNSSuffix`, not an exact direct `MarginServerDNSName`
    - newer auth/margin config review now also gives the strongest concrete answer yet for where connection is initiated from:
      - auth-side launcher owner path:
        - owner root at `0x4f78b8`
        - owner construction path stores `0x4f78b8 = esi`, then immediately calls `0x43b300`
        - `0x43b300` allocates/initializes helper/state objects at `0x4f7868 / 0x4f786c / 0x4f7870 / 0x4f78a0`
        - auth DNS current string consumed from `0x4f7b14`
        - auth port consumed from `0x4f7a50`
        - `0x43909f -> 0x41d170` constructs `CMessageConnection`, builds endpoint at owner `+0x5c`, then calls `connection->+0x1c(owner+0x5c)`
      - margin-side launcher owner path:
        - margin suffix consumed from `0x4d6814`
        - margin port consumed from `0x4d669c`
        - `0x439345 / 0x43936b / 0x43938e / 0x4393bf -> 0x41e500` constructs `CMessageConnection`, builds endpoint at owner `+0x6c`, then calls `connection->+0x1c(owner+0x6c)`
        - those calls are reached through owner-state dispatch in `0x439300`, which consults `[owner+4]->vtable+0x18` and then uses owner vtable surfaces `+0xe0 / +0xfc / +0x10c` plus owner fields `+0xcc8 / +0x12c / +0x104`
      - with the current method mapping, that virtual `+0x1c` remains best read as the connection-oriented ensure-connected / engine-`Connect` wrapper
      - so connection init currently looks launcher-owned and higher-level-owner-driven, not a trivial direct raw-global `0x4d6304->Connect(...)` call
      - newer implementation milestone: the scaffold can now make a **real auth TCP connection** from the launcher side when running the binder/stub path
        - current config defaults now mirror recovered binary strings:
          - auth host default = `auth.lith.thematrixonline.net`
          - auth port default = `11000`
          - margin suffix default = `.lith.thematrixonline.net`
          - margin port default = `10000`
        - current opt-in env knobs:
          - `MXO_BEGIN_AUTH_CONNECTION=1`
          - `MXO_BEGIN_MARGIN_CONNECTION=1`
          - optional overrides:
            - `MXO_AUTH_SERVER_DNS`
            - `MXO_AUTH_SERVER_PORT`
            - `MXO_MARGIN_SERVER_DNS`
            - `MXO_MARGIN_SERVER_SUFFIX`
            - `MXO_MARGIN_SERVER_PORT`
            - `MXO_MARGIN_ROUTE_PREFIX`
        - current verified run result on binder/stub path:
          - `CLTLoginMediator::BeginAuthConnection() authHost='auth.lith.thematrixonline.net' port=11000 -> 0x00000001`
        - current margin attempt with guessed `vector.lith.thematrixonline.net:10000` still fails (`0x00000000`), so exact margin-host derivation remains unresolved
        - newer exact-host override check also shows the raw socket/connect path itself is no longer the blocker there:
          - with `MXO_MARGIN_SERVER_DNS=auth.lith.thematrixonline.net`, the margin-side path also returns `0x00000001`
          - so the remaining margin gap is host derivation/fidelity, not basic launcher-side TCP capability
        - important current runtime limitation after that auth-connect milestone:
          - real auth socket connect alone was not yet enough by itself to make the client-visible queue consumer branch go live
        - newer queue/work implementation milestone after that:
          - the scaffold can now deliberately enqueue queue0C `(workItem, context)` pairs after successful auth/margin connect attempts
          - deliberate `RunClientDLL` runs now show queue0C cursor divergence before first consume and then client-side callback/release activity on those queued items
          - current runtime evidence on that path now includes:
            - `raw message-connection context OnOperationCompleted ...`
            - `releasing queued work item ...`
          - with auth connect enabled, one queued connect-status item is currently consumed on that path
          - with auth connect + exact-host-overridden margin connect enabled, two queued connect-status items are currently consumed in order on that path
          - newer static narrowing now also explains why that still does not automatically produce the first real outbound auth message:
            - original `Connect` success path `0x4329b9..0x4329cc` constructs `0x435050(0x7000001)` which is a **type-2** status work item, then enqueues `(workItem, connection, 0)`
            - auth-side startup path `0x41d170` builds a **derived** `CMessageConnection` object with vtable `0x4afef0`
            - margin-side startup path `0x41e500` builds another derived connection object with vtable `0x4aff38`
            - those derived families use wrapper `OnOperationCompleted` entries `0x449a70` / `0x44af60` on top of base `0x4490c0`
            - newer focused review now also resolves the previously vague `CMessageConnection +0x7c / +0x80` helper objects much further:
              - ctor helper `0x436080` builds a `0x24` event+critical-section helper object
              - current best helper shape:
                - main `+0x00` = `SetEvent(...)`
                - main `+0x04` = `WaitForSingleObject(timeout)` with lock release/reacquire
                - embedded lock helper at `+0x04`
                - event handle at `+0x20`
              - base completion dispatcher `0x4490c0` uses them as optional notifier helpers:
                - work type `2` -> signal `connection+0x7c`
                - work type `1` -> signal `connection+0x80`
              - current best subtype read is:
                - `+0x7c` = type-2 status/connect-style completion helper
                - `+0x80` = type-1 close/terminal completion helper
              - important negative narrowing on the auth/margin startup path:
                - `0x41d170 / 0x41e500 -> 0x4417e0 -> 0x448b40(flag=0)` leaves both `+0x7c` and `+0x80` as **NULL** there
                - so current auth/margin type-2 connect-status completion falls through `0x449a70 / 0x44af60` into the owner callback / fallback chain instead of being resolved inside those helper objects alone
            - important correction from newer message-code review:
              - later auth helper body `0x4401a0` handles raw auth message code `0x0b`
              - auth message-name mapping at `0x41bd10` uses `code - 6`, so raw `0x0b` resolves to **`AS_AuthReply`**, not `AS_GetPublicKeyReply`
              - margin/loading helper body `0x440320` similarly handles raw code `0x10`, which resolves through `0x41bf70` to **`MS_LoadCharacterReply`**, not an initial connect request
              - so those callbacks are currently best treated as **later incoming packet handlers** on the type-3 receive path, not direct proof of the first outbound request after connect
            - newer auth-side fallback-chain review now gives the strongest corrected answer yet for where the first auth send does **not** come from:
              - `0x449a70` runs as `0x4490c0 -> owner +0x17c -> fallback 0x448a60`
              - owner `+0x17c` is thunk `0x41f260`, which forwards to the owner's current helper/state object at `+0x10`, then jumps to helper vtable `+0x14`
              - so `0x4401a0` is not the generic owner callback itself; it is later helper `0x4f7890` (vtable `0x4b512c`) slot `+0x14`
              - that later helper only meaningfully handles raw `0x0b` / `AS_AuthReply`
              - on its success branch it parses via `0x43a330`, updates owner `+0x80`, appends a small record under owner `+0x684`, mirrors the current record index to owner byte `+0xcc8`, then reaches `0x41b450(0x0b)` and string-backed `CLTLoginMediator::PostEvent(0x14)`
              - newer helper-side follow-up on that same success branch is still negative for first-send purposes:
                - `0x41b450(0x0b)` selects helper `0x4f7894` (vtable `0x4b5154`)
                - helper `+0x8` / `0x43c020` prepares owner-side data and posts event `0x15`
                - it still does **not** show the missing first auth send
              - on its failure branch it reaches `0x41b450(3)` and string-backed `CLTLoginMediator::PostError(0x0b)`
              - fallback `0x448a60` is only the string-backed generic logger `Got unhandled op of type %d with status %s`
              - so the missing first faithful outbound auth/message send is currently best treated as **not** originating from later helper body `0x4401a0` and **not** from `0x448a60`
            - newer helper-family tracing now also sharpens one important auth-ownership point the user called out:
              - even if `+Username/+Password` reach client-facing argv/state, that still does **not** imply `client.dll` owns auth
              - current later concrete launcher-side auth-channel send remains:
                - helper object `0x4f78a0`
                - send body `0x43b830`
                - wrapper path `0x41af60 -> auth connection +0x24 / 0x41cf30 -> +0x28 / 0x448cf0 -> 0x448a00 -> 0x449d20 -> engine +0x20`
                - packet raw code `0x35` = **`AS_GetWorldListRequest`** through the auth table
              - but newer static narrowing now ties the **earlier** launcher-owned credential/bootstrap lead down much further than before:
                - direct code xrefs to `0x41af60` still only show that later `0x43b830` world-list path
                - current strongest earlier lead is helper `0x4f7870` selected through `0x41b450(2)`
                - helper `+0x08 / 0x439210` first gates on `0x41b490()` and falls back to `0x41b450(1)` if auth is not connected yet
                - on the connected branch it gathers launcher-owned owner data through owner `+0x168`, `+0x20`, and `+0x38`, then calls `0x448050`
                - `0x448050` is currently only xref'd from `0x439210`
                - `0x448050` now also resolves more concretely than before:
                  - it runs as a method on the extra owner child allocated by `0x41290` / base ctor `0x45500` and stored at owner `+0x680`
                  - that phase-2 bootstrap object is size `0x11c`
                  - it copies three string sources plus two 16-byte blocks into bootstrap state and stores an outbound send target at bootstrap `+0x50`
                  - crucial correction: the branch at `0x44811e` is not just a loose byte-ish mode flag but the nullness of **pointer `+0xa0`** inside that bootstrap object
                - `0x448050` then branches by bootstrap helper pointer `+0xa0` into two launcher-owned outbound packet builders that both send indirectly through bootstrap `+0x50 -> +0x24`:
                  - `0x447eb0` is taken when bootstrap helper pointer `+0xa0 == NULL` and builds/sends raw auth code `0x06` -> strongest current **`AS_GetPublicKeyRequest`** candidate
                  - `0x4474f0` is taken when bootstrap helper pointer `+0xa0 != NULL` and builds/sends raw auth code `0x08` -> strongest current **`AS_AuthRequest`** candidate
                  - `0x4474f0` also emits a later auxiliary raw `0x1b` packet on that same indirect path
                - newer bootstrap-object follow-up now also tightens later field meaning:
                  - `0x429b0` uses bootstrap helper pointer `+0xa0 -> +0x1c` on a later incoming path
                  - it writes 16-byte challenge-derived material to bootstrap `+0x85 .. +0x94`
                  - `0x41470` then derives/caches a dword-ish token at bootstrap `+0x9c`
                  - that same `+0x9c` value is later used by both raw `0x06` and raw `0x08` send builders
              - important channel-specific correction from the same pass:
                - margin-side wrapper traffic must be decoded through the margin table
                - raw `0x06` on `0x41af70` = **`MS_GetClientIPRequest`**, not auth `AS_GetPublicKeyRequest`
              - so the auth-side bootstrap send is no longer just a generic unresolved blank before world-list:
                - it still sits **before** the later `AS_GetWorldListRequest` path
                - the highest-value concrete target remains the **phase-2 helper chain** `0x4f7870 -> 0x439210 -> 0x448050 -> (0x447eb0 raw 0x06 | 0x4474f0 raw 0x08)`
                - and the remaining unknown is now narrower:
                  - not whether `+0xa0` matters at all
                  - but what exact helper object is stored at bootstrap `+0xa0`, what exact send-target object is stored at bootstrap `+0x50`, and what exact source-object family is copied into the bootstrap object before that branch
            - the current diagnostic raw-context callback is therefore still bypassing the original post-connect auth/margin completion chain where the next faithful outbound message likely lives
          - current runtime log now makes that narrowing explicit by logging:
            - `routed auth type-2 connect-status payload=0x07000001 into CLTLoginMediator scaffold -> handled=1 nextOutboundRequest='phase2-bootstrap: +0xa0 NULL => AS_GetPublicKeyRequest, non-NULL => AS_AuthRequest' laterIncomingReplyAnchor='AS_AuthReply'`
          - newer env-gated diagnostic bootstrap experiment now also gives the first concrete outbound/reply pair on this launcher-owned auth path:
            - command:
              - `MXO_FORCE_RUNCLIENT=1 MXO_BEGIN_AUTH_CONNECTION=1 MXO_DIAGNOSTIC_SEND_AS_GETPUBLICKEY=1 MXO_DIAGNOSTIC_AUTH_LAUNCHER_VERSION=76005 MXO_DIAGNOSTIC_AUTH_CUR_PUBLIC_KEY_ID=0 MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Vector make run_binder_both`
            - scope label:
              - this is a **diagnostic bootstrap experiment**, not a claim of faithful original-equivalent launcher progression yet
              - `76005` is a practical guessed launcher-version value from current `7.6005`, not yet a fully recovered canonical owner-field proof
            - current result from that run:
              - env-gated send log:
                - `auth bootstrap experiment rawCode=0x06 request='AS_GetPublicKeyRequest' launcherVersion=76005 currentPublicKeyId=0 byteCount=10 -> sendResult=0x00000001`
              - a new queued auth receive item then appears on the same runtime path:
                - `queued connection-status work item label='AuthReceivePacket' ... type=3 payload=0x00000198`
              - receive preview + framing decode now log:
                - `received-bytes label='AuthConnection' total=408 preview=81 96 07 ... 04 00 00 00 12`
                - `auth receive framing payloadLength=406 headerBytes=2 rawCode=0x07 likelyMessage='AS_GetPublicKeyReply'`
              - newer static formatter alignment with `0x44e20` now also makes that preview itself informative:
                - `81 96` = 2-byte length prefix for payload `0x0196 = 406`
                - payload byte `0x07` = `AS_GetPublicKeyReply`
                - next dword `00 00 00 00` = plausible reply status `0`
                - next dword = plausible `CurrentTime`
                - next dword `04 00 00 00` = plausible `PublicKeyId = 4`
                - next byte `0x12` aligns with the later formatter's `KeySize` field
            - current strongest reading from that pair:
              - the small-payload framing model derived from `0x448a00` is now runtime-supported enough to matter
              - the raw `0x06` send shape from `0x447eb0` is now runtime-supported enough to matter
              - and the server reply beginning with raw `0x07` strongly supports the current `AS_GetPublicKeyRequest -> AS_GetPublicKeyReply` read on this launcher-owned auth path
            - important restraint:
              - this still does **not** prove the full faithful original field population yet
              - but it materially upgrades the auth-bootstrap work from pure static suspicion to a live send/reply diagnostic foothold
          - this is still binder/scaffold progress, not yet faithful original producer semantics
          - newer non-blocking receive polling is also now wired into the helper `+0x60` runtime surface, but current timed auth-connect runs have not yet produced logged type-3 receive work items on that path
          - fresh validation reruns after the newer auth-side owner/fallback-chain narrowing still did **not** move the runtime surface yet:
            - plain `make run_binder_both` still stops at clean `InitClientDLL returned: 1` with `RunClientDLL` gated
            - deliberate auth runtime rerun
              - `MXO_FORCE_RUNCLIENT=1 MXO_BEGIN_AUTH_CONNECTION=1 MXO_ARG7_SELECTION=0x0500002a MXO_MEDIATOR_SELECTION_NAME=Vector make run_binder_both`
              - still consumes exactly one queued auth type-2 connect-status item
              - now logs `nextOutboundRequest='phase2-bootstrap: +0xa0 NULL => AS_GetPublicKeyRequest, non-NULL => AS_AuthRequest'`
              - still falls back to the same mediator `+0x2c` + arg5 helper `+0x60` slot `0/1` idle loop after that first consume
              - still shows no logged type-3 receive work items before timeout
            - so current runtime evidence still does **not** contradict the newer static negative conclusion that `0x4401a0` / `0x448a60` are not the missing first-send origin
          - user now also subjectively reports this is the longest the current scaffold has remained in the in-game `Loading Character` phase so far; treat that as encouraging but still secondary to harder runtime markers like new queue work / new callbacks / first real receive item
  - newer deliberate rerun after partially wiring slots `1/2/6/7/8/12` into `src/liblttcp/` did **not** change the observed runtime surface yet:
    - still only mediator `+0x2c`
    - still only arg5 helper `+0x60` slot `0/1`
    - still no live slot `6/7/8/12` traffic in the current runtime loop
    - still empty queue0C / queue34 cursor state
  - user subjectively observed that the in-game **Loading Character** phase seemed to stay visible/useful longer after the recent class-wiring cleanup, but current logs do **not** yet prove a deeper runtime transition there; treat that as suggestive only until a new surface/state change/crash signature confirms it
- visible current runtime state still includes an in-game **Loading Character** phase with loading bar / status text on the deeper path
- static ordering still explains why that UI is compatible with the currently logged mediator depth:
  - `client.dll:0x62170f2a` pushes the string `"Loading Character"`
  - and the same block then calls `arg6->+0xec` at `0x62170f48`
  - so seeing that UI proves the client reached the pre-`+0xec` loading/status phase, but does **not** yet prove later loading-character consumers such as the `CreateCharacterWorldIndex` read at `0x62054cbd`
- the deepest stable logged mediator sequence now reaches:
  - `GetWorldOrSelectionName()`
  - `GetProfileOrSessionName()`
  - repeated `GetSelectionDescriptor(...)`
  - `AttachStartupContext(first)`
  - `ProvideStartupTriple(netShell, netMgr, distrObjExecutive)`
  - `ConsumeSelectionContext(...)` at `+0xec`
- the old late `arg2+2` family remains canonically documented in:
  - `../../docs/client.dll/InitClientDLL/CRASH_EIP_003E2B82.md`
- but it is now best treated as a **resolved scaffold bug** in the current path, caused by the mediator `+0x5c` / `+0x60` cleanup mismatch rather than by an unresolved mysterious late unwind alone
- widening arg5 reconstruction now includes:
  - full recovered primary vtable surface `0x4b2768` slots `0..12`
  - evidence-backed empty-path modeling for slot `5` (`0x431840`)
  - corrected `+0x80` / `+0x8c` sentinel-headed container interpretation
- but even after that wider arg5 work:
  - no observed arg5 traffic has appeared before the late crash from:
    - primary slots `5..10`
    - primary slots `11..12`
    - embedded helper surfaces at `+0x5c`, `+0x60`, `+0x98`
- the in-launcher `ret` bypass was useful for validation, but only negatively:
  - it can step over the immediate `arg2` landing
  - the popped target is just stale startup-frame `arg3 hClientDll = 0x62000000`
  - execution then immediately faults at `client.dll+0x3`
  - it still does **not** reveal later arg5 method traffic
- newer stack logging now preserves the intended 8-argument `InitClientDLL` frame before the call and compares it against the crash-time stack:
  - on the late `arg2+2` crash, current stack top matches stale startup-frame values in order:
    - `esp[0] = arg3 hClientDll`
    - `esp[1] = arg4 hCresDll`
    - `esp[2] = arg5 launcherNetworkObject`
    - `esp[3] = arg6 ILTLoginMediatorDefault`
  - with non-zero arg7 experiments, `esp[4]` also matches `arg7 packedArg7Selection`
  - this further supports the current corrupted-return-chain interpretation
- arg7/method-surface scaffolding is now slightly less shallow:
  - the diagnostic mediator now exact-matches the configured selected world / variant for `+0xfc / +0x100 / +0xe4 / +0x40` instead of generically accepting every in-range index
  - `+0x3c` now returns the configured selected world id instead of hardcoded `0`
  - optional env knobs now exist for diagnostic selection-shape experiments:
    - `MXO_MEDIATOR_WORLD_TYPE`
    - `MXO_MEDIATOR_VARIANT_STATE`
- first non-zero arg7 probe result from that stricter model:
  - with `MXO_ARG7_SELECTION=0x0500002a` and `MXO_MEDIATOR_SELECTION_NAME=Vector`
  - launcher-side sibling-slot rebuild still resolves `+0xfc/+0x100/+0xe4` successfully for world `0x2a`, variant `0x05`
  - a fresh static pass now explains the later client `+0x40` request shape:
    - inside `client.dll:0x62170dc1..0x62170e59`, the client reuses the original arg7 stack slot as scratch and overwrites only its low byte with the high-8-bit variant value
    - so `arg7=0x0500002a` becomes client-side `selectionIndex=0x05000005` before later path-building helpers call `arg6->+0x40`
    - importantly, the same block also stores the original low-24-bit selection id separately via `push esi ; mov ecx, 0x629e1c7c ; call 0x620011e0` **before** that scratch mutation
  - the diagnostic mediator now accepts that scratch-shaped `+0x40` request and returns the configured descriptor (`matchMode=arg7-scratch-shape`)
  - current best reading is therefore that the client expects **both**:
    - a stable persisted low-24-bit selection id somewhere else
    - specifically through the nearby store path rooted at `0x629e1c7c` via `call 0x620011e0`
    - and the later scratch-shaped `+0x40` lookup key
  - newer static follow-up now identifies that `0x629e1c7c` state more concretely:
    - static initializer `client.dll:0x627c5320` constructs a client-side console-int object named `CreateCharacterWorldIndex` at `0x629e1c7c`
    - constructor `0x620022f0` seeds it with default/current `0`
    - base console-var ctor `0x622a2270` zeroes callback slots `+0x20 / +0x24 / +0x28`
    - current direct xref search found only one concrete direct read of its current value field `0x629e1cb0` at `client.dll:0x62054cbd`
    - surrounding strings there (`CharCreate_2_Finish`, `CharCreate_2_Back`, `Loading Character`) still tie it to character/loading flow rather than a simple mediator method surface
    - important ordering correction from newer static review: the user-visible `"Loading Character"` status text comes from earlier `client.dll:0x62170f2a`, immediately before the already-observed `arg6->+0xec` call at `0x62170f48`
    - so the current visible loading-bar state keeps this area interesting, but does **not** yet prove we reached the later `CreateCharacterWorldIndex` consumer at `0x62054cbd`
    - to avoid missing the next loading-phase transition, the diagnostic mediator now also exposes/logs slot `+0x120`; follow-up rerun `crash_62` still showed no `+0x120` traffic before the same late crash
  - so the low-24-bit arg7 path now looks more like client-owned persisted console/config state than another unresolved arg6 method contract — but it may still be materially relevant on the current failing loading-character path
  - but this still did **not** move the late crash family (`~/MxO_7.6005/MatrixOnline_0.0_crash_60.dmp`, `EIP=0x003e5e8a`)
- newer evidence-backed mediator corrections now tried without moving this crash family:
  - the scaffold now copies the full `0xb4` `+0xec` selection/config handoff object into stable mediator-owned storage
  - `+0x38` is now treated as the client's `Profiles\%s\...` root string input and returns the profile/session-style name (`morgan`)
  - that `+0x38` correction materially changed on-disk side effects by creating `~/MxO_7.6005/Profiles/morgan/aui.cfg`
  - newer static work now also explains two deeper details of this path:
    - `client.dll:0x62195ff0` uses the `+0x40` descriptor payload fields at `+3` (name pointer) and `+7` (selection id) for the `Profiles\%s\%s_%X\` suffix formatting
    - `client.dll:0x62170de2..0x62170e3b` stores the zero-extended arg7 high-8-bit variant value into the first dword of the later `+0xec` handoff object
  - in the non-zero arg7 probe, that matches current copied `selectionContext[0] = 0x00000005`
  - current runtime log now explicitly confirms that field shape:
    - `DIAGNOSTIC: selectionContext[0]=0x00000005 (configuredVariant=0x05 configuredWorld=0x00002a)`
  - but the late crash family still remained
- current best remaining launcher-owned suspects are:
  - still-incomplete arg7 low-24-bit / selection-id state
  - the launcher-owned arg7 selection-resolution chain around `0x40d763..0x40d810`
  - broader launcher-owned preprocessing / globals from `0x409950`
  - another later launcher/client ownership mismatch that redirects control into current arg2 storage
  - launcher-owned auth/crashreporter state is also now more concretely split than before:
    - original launcher `0x409220..0x409254` seeds crashreporter defaults from mediator methods individually
    - `+0x5c -> 0x42ee50 -> 0x4d7418` = crashreporter username
    - `+0x60 -> 0x42ee80 -> 0x4d7424` = crashreporter password
    - `+0x58 -> 0x42ede0 -> 0x4d73b8` = crashreporter `PromptForSecurId` byte-ish flag
    - newer scaffold cleanup removes the fake launcher/auth split for username/password storage, now keeps auth values directly as auth-owned state, and wires mediator `+0x60` from configured auth password while preserving the caller-clean wrapper shape
    - new disposable-credential validation now confirms that path end-to-end on the binder/scaffold init-success branch:
      - with username `pwcheck`
      - with password `PW_TEST_7Q9X2M4K`
      - and deliberate post-init crash `MXO_DIAGNOSTIC_CRASH_AFTER_INIT_SUCCESS=1`
      - `crashreporter_stub.log` recorded exactly:
        - `+Username "pwcheck"`
        - `+Password "PW_TEST_7Q9X2M4K"`
        - `+PromptForSecurId "1"`
    - so crashreporter-side auth propagation is no longer the active blocker
- important newer arg7 clarification from static review remains:
  - the launcher-global root at `0x4d3584` is not just an unknown generic service object
  - initializer `0x496480..0x496491` registers `0x4d3584` through the same `0x4030d0` wrapper using the same string `"ILTLoginMediator.Default"`
  - current next arg7 focus should therefore be the sibling `ILTLoginMediator.Default`-style slot at `0x4d3584` and its `+0xfc / +0x100 / +0xe4` selection queries, not only registry fallback

Current original-launcher runtime validation note:
- the original `~/MxO_7.6005/launcher.exe` now successfully logs into the live game on this machine after manual EULA acceptance
- user also switched it to enter the loading area instead of the live game world
- that is useful differential evidence that the current host Wine/DXVK/runtime environment can support a successful launcher-driven login path
- practical caveat: original-launcher runs are manual and may block on UI/EULA interaction, so they are not suitable as unattended automation steps

Diagnostic-only forced runtime result:
- older forced-runtime reference dump: `~/MxO_7.6005/MatrixOnline_0.0_crash_73.dmp`
- crash: `client.dll+0x3b3573`

That forced crash is useful for diagnostics, but it is **not** original-equivalent behavior.

## Build / Install / Run

Build:
```bash
cd /home/morgan/mxo/code/matrix_launcher
make
```

The active launcher is built directly to:
- `~/MxO_7.6005/resurrections.exe`

Safe run:
```bash
make run
```

Forced incomplete-init experiment:
```bash
make run_incomplete_init
```

Forced runtime after failed init (diagnostic only):
```bash
make run_forced_runtime
```

Arg6 stub experiment:
```bash
make run_stub_mediator
```

Combined arg5+arg6 diagnostic experiments:
```bash
make run_stub_both
make run_binder_both
```

Diagnostic-only late-crash bypass experiment:
```bash
MXO_ARG2_RET_BYPASS=1 MXO_ARG2_RET_BYPASS_MAX=4 make run_binder_both
```

Latest crash dump summary:
```bash
make crashdump
```

## Key Files

Code:
- `src/resurrections.cpp` - startup orchestration / DLL loading / InitClientDLL frame
- `src/diagnostics.cpp` - mediator stub, launcher-object stub, window tracing
- `src/diagnostics.h`
- `src/liblttcp/ltthreadperclienttcpengine.h` / `.cpp` - starter original-name engine skeleton (`MonitorPort`, `UDPMonitorPort`, provisional slot-3 `MonitorEphemeralUDPPort`, `Connect`, `Close`, `SendBuffer`, `CleanupConnection`) now partially wired from the diagnostics arg5 scaffold for slots `1` / `2` / `6` / `7` / `8` / `12`
- `src/liblttcp/lttcpconnection.h` / `.cpp` - starter original-name TCP connection skeleton (`Close`, `SendBuffer`, `OnReceive`) now treated as the current best base connection layer under `CMessageConnection`
- `src/liblttcp/cmessageconnection.h` / `.cpp` - starter original-name message-connection skeleton (`SendPacket`, `OnOperationCompleted`) that now mirrors the best current read of the queue0C `context` object family and now exposes sidecar `EnsureConnected()` / `CloseConnection()` wrappers used by the diagnostics arg5 slot-6/slot-7 paths, while `SendPacket(...)` now backs the diagnostics slot-8 path
- `src/ltlogin/loginmediator.h` / `.cpp` - scaffold-first launcher-side login/controller skeleton around the current best `CLTLoginMediator`/`0x4f78b8` read, including auth/margin server config anchors, helper-family scaffolding for `0x4f7868/6c/70/0xa0`, and source-level placeholders for the recovered margin-route owner vtable surfaces `+0xe0 / +0xfc / +0x10c`
- `src/ltlogin/loginstates.h` / `.cpp` - scaffold-first login-state skeletons for string-backed methods like `CLTLoginState_WorldListPending::AuthMessageDispatch()` and `CLTLoginState_AuthenticatePending::AuthMessageDispatch()`, now also preserving original recovered log/error strings directly in source as future implementation/logging anchors
- `Makefile`
- runtime executable: `resurrections.exe`

Canonical docs:
- `../../docs/launcher.exe/client_dll_loading/LOADING_SEQUENCE.md`
- `../../docs/client.dll/InitClientDLL/README.md`
- `../../docs/client.dll/RunClientDLL/README.md`
- `../../docs/launcher.exe/startup_objects/README.md`

## Project-Specific Rules

- Do not treat old test harnesses as the solution architecture
- Do not add `client.dll` memory injection to the intended path
- Do not treat a forced `RunClientDLL` after failed `InitClientDLL` as original-equivalent behavior
- Prefer original launcher/client names when a string-backed or strong static name is available (`MonitorPort`, `UDPMonitorPort`, `Connect`, `Close`, `SendBuffer`, `CleanupConnection`, `AcceptThread`, `WorkerThread`, `CLTLoginMediator`, `CLTLoginState_AuthenticatePending`, etc.) instead of inventing fresh generic labels
- When a recovered original log/error string clearly identifies a method or state path, prefer reusing that original string as a source-level anchor in scaffolds/comments and later logging rather than inventing fresh text
- Prefer pushing interface/class/method recovery into source scaffolds first (with tight inline comments) and reserve `../../docs/` for experiment evidence, runtime behavior, and cross-component conclusions rather than duplicating interface descriptions in markdown
- Be diligent about experiment documentation: every meaningful rerun, crash change, stable non-change, or new disassembly-backed interpretation must update the relevant canonical docs in `../../docs/` as part of the same work, not later
- Record negative results too when they narrow the search, but keep them in canonical component docs rather than scattered duplicate notes
- When a crash becomes a recurring reference, prefer canonical doc names keyed by a stable signature such as faulting `EIP` / `module+offset` rather than transient dump numbers alone; record the specific dump filenames inside the doc body
- For PE inspection tasks, treat `python3 pefile` as a normal supplementary RE tool for:
  - VA/RVA/file-offset conversion
  - vtable neighborhood inspection
  - nearby string/rdata inspection
  - quick binary-structure checks
  - but keep disassembly/debugger evidence as the primary source for control flow, calling convention, and semantic claims

## Immediate Next Work

1. Recover the arg7 selection-resolution chain around `0x40d763..0x40d810`, `0x48baea`, and the sibling `ILTLoginMediator.Default`-style slot at `0x4d3584` (`+0xfc / +0x100 / +0xe4`) instead of treating `Last_WorldName` alone as the derivation
2. Follow the new `+0x40` scratch-shape explanation deeper:
   - determine how the client expects the mutated arg7 scratch request to map back to persisted low-24-bit selection id / descriptor data after `0x62170dc1..0x62170e59`
   - stop assuming that accepting the scratch-shaped request alone is enough
3. Reconstruct more of `0x409950` launcher-side preprocessing, especially `options.cfg` side effects and launcher-global state derived before `InitClientDLL`
4. Follow the **new post-fix blocker** now that the old late `arg2+2` crash family no longer reproduces on the current binder path:
   - preserve the now-confirmed original success contract from `launcher.exe:0x40a5a9 / 0x40a624 / 0x40a6be` (`test eax,eax ; jg ...`) and `0x40a6fd` (`al = 1` on overall success)
   - treat positive `InitClientDLL` / `RunClientDLL` / `TermClientDLL` returns as the original success shape, not an anomaly
5. Trace the stable deliberate `RunClientDLL` loop now reachable on the clean binder path:
   - `RunClientDLL -> 0x62006c30` repeatedly polls mediator `+0x2c`
   - then drives arg5 through `0x62532130 -> 0x62531c10(1)`
   - treat that as the **non-blocking consumer** side of the same engine-family logic seen in launcher `0x436b10`, not merely as arbitrary queue comparisons
   - keep the queue-field mapping precise there:
     - `+0x0c` = queue0C `current0`
     - `+0x1c` = queue0C `current1`
     - `+0x34` = queue34 `current0`
     - `+0x44` = queue34 `current1`
   - follow the original producer side now identified at `0x436820 -> 0x436670`:
     - acquires `+0x60`
     - enqueues an 8-byte pair into queue0C / queue34
     - releases `+0x60`
     - signals `+0x5c` on empty->non-empty transition
   - the representative set is no longer enough; all currently identified concrete producer xrefs have now been read and should be treated as the active reference set:
     - `0x4302d5`
     - `0x43051f`
     - `0x43067f`
     - `0x4306a7`
     - `0x4309da`
     - `0x4309ef`
     - `0x430c25`
     - `0x430d71`
     - `0x430d94`
     - `0x430da8`
     - `0x4315b0`
     - `0x4325aa`
     - `0x4329cc`
     - `0x432d86`
     - `0x432dc1`
     - `0x432dd7`
     - internal lifecycle/self-calls: `0x436a0e`, `0x436fa8`
     - looped submit-and-wait path: `0x449d8a`
   - current concrete startup/runtime producer evidence passes third-arg `0`, so prioritize understanding the **queue0C** feed path first
   - keep the newer semantic narrowing in mind while doing that:
     - queue0C now looks like a launcher-owned **network-engine event/status queue**
     - not a generic arbitrary job list
     - concrete recovered producer families now already touch `recvfrom`, `socket`, `setsockopt`, `bind`, `listen`, and `connect`
   - determine what the queued pair means semantically:
     - first dword = work-item-like object (`0x435090` / `0x435010` / `0x435050` / `0x435070` families)
     - second dword = owner/context pointer or paired object
   - keep the current concrete xref taxonomy straight while doing that:
     - some producers enqueue existing objects
     - some enqueue fresh small command/status objects
     - some do paired multi-submit sequences
     - some fall back to null submissions
     - `0x449d8a` currently looks like a submit-and-wait loop
   - current highest-value semantic anchors inside that taxonomy are now:
     - `0x4302d5` = recvfrom-adjacent / packet-side producer
     - arg5 primary slot `1` / `0x431ce0` = `MonitorPort` (TCP listen/accept-side endpoint-keyed `+0x80` population)
     - arg5 primary slot `2` / `0x4325d0` = `UDPMonitorPort` (UDP socket/bind family path in the same engine)
     - arg5 primary slot `3` / `0x436000` = provisional UDP-monitor helper / local-port query wrapper
     - arg5 primary slot `6` / `0x4328a0` = `Connect` (TCP socket/bind/connect family producer path)
     - arg5 primary slot `7` / `0x42f970` = `Close`
     - arg5 primary slot `8` / `0x42fbd0` = `SendBuffer`
     - arg5 primary slot `12` / `0x4316a0` = `CleanupConnection`
     - queue0C `context` is now likely a `CMessageConnection`-family object on important paths
     - current best `CMessageConnection` callback mapping is now:
       - `+0x10` / `0x4490c0` = likely `OnOperationCompleted(workItem)`
       - `+0x20` / `0x449d20` = likely `SendPacket(...)` -> engine `+0x20` / `SendBuffer`
       - `+0x1c` / `0x449cd0` = likely endpoint-update / ensure-connected wrapper -> engine `+0x18` / `Connect`
       - `+0x0c` / `0x449ca0` = likely close/abort wrapper -> engine `+0x1c` / `Close`
     - newer ctor/vtable review now also shows:
       - base `CLTTCPConnection` vtable = `0x4b8018` (string-backed by nearby `CLTTCPConnection::OnReceive()` text)
       - derived `CMessageConnection` vtable = `0x4b7928`
       - current live slot-6/7/8 path is connection-object-based through that family, not just raw primitive endpoint args
     - `0x435050(payload)` = coded status/result item family (`0x700000x`-style)
   - keep the now-identified next consumer milestone in mind:
     - once queue work exists, client `0x62531e31..0x62531fe7` should reach arg5 primary slot `12` at vtable offset `+0x30`
     - original launcher consumer `0x436d31..0x436ee7` then treats the dequeued pair as `(workItem, context)`, reads `[workItem+0x04]`, calls slot `12(context)`, and then calls `context->+0x10(workItem)`
     - that means the next fidelity gap is not just “feed any queue entry” but “feed the right **workItem + context** pair family so the later slot-12 / context callback chain is meaningful”
   - current best concrete producer-side preconditions to trace before runtime are now:
     - `MonitorPort` seeding endpoint-keyed `+0x80` with `AcceptThread` payloads
     - `UDPMonitorPort` seeding pointer-keyed `+0x8c` with `WorkerThread` payloads in state `2`
     - `Connect` seeding pointer-keyed `+0x8c` with `WorkerThread` payloads in state `1`
     - `CMessageConnection`-family objects capturing the engine pointer, then driving `Connect` / `Close` / `SendBuffer`, and enqueueing `(workItem, self, 0)` into queue0C
   - determine which launcher-owned startup/runtime state should cause those producer paths to become live beyond the present idle loop where both queues still show `current0 == current1`
6. Trace the persisted low-24-bit selection-id path rooted at `0x629e1c7c` / `0x620011e0` now that it is identified as client-side `CreateCharacterWorldIndex`, and determine how its later consumers (especially on the loading-character path around `0x620547c0..0x62054eac`) depend on launcher-owned state before or alongside the later scratch-shaped `+0x40` lookup
7. Improve semantic validation of the post-`+0xec` `0xb4` selection/config handoff object instead of treating it as only an opaque copied buffer
8. Reconstruct deeper `0x4d6304` state on the original path, but stop assuming the currently recovered arg5 slots alone explain the late crash
9. Revisit arg8 / nopatch-derived flag-byte handling once the arg7 / preprocessing path is less incomplete
10. Keep tracing what `0x402ec0` minimally sets up
11. Continue deliberate `RunClientDLL` experiments on the now-clean positive-return path, but keep them clearly labeled as binder/scaffold progress rather than faithful original-equivalent success until the launcher-owned arg5/arg6/arg7/pre-client state is reconstructed more faithfully
12. If the deliberate path keeps avoiding fresh crashes, treat progress measurement as shifting from crash signatures to **new runtime surfaces/state changes** instead:
   - new mediator slots
   - new arg5 primary-slot traffic
   - first non-empty queue0C / queue34 state
   - first reach of slot `12`
   - first class-backed sidecar activity that actually becomes live in logs
   - only add dwell-time/UI timing instrumentation if those stronger runtime markers stop moving for a while
13. Current highest-value next runtime task after the auth-connect + queue-consume milestone:
   - identify the first **real outbound auth/message send** that should happen after launcher-side auth connect
   - current best narrowing is still that this sits behind the original **type-2 connect-status completion chain**, not behind raw TCP connect alone:
     - `0x4329b9..0x4329cc` -> `0x435050(0x7000001)` -> queue0C `(workItem, connection, 0)`
     - auth/margin derived connection families still matter there, but do **not** over-read later packet handlers as proof of the first send:
       - auth derived vtable `0x4afef0`, later packet wrapper `0x449a70`, later incoming auth helper-state anchor `0x4401a0 -> AS_AuthReply`
       - margin derived vtable `0x4aff38`, later packet wrapper `0x44af60`, later incoming loading anchor `0x440320 -> MS_LoadCharacterReply`
   - but the earlier `+0x7c / +0x80` helper-object target is now partly resolved and should no longer be treated as the main mystery by itself:
     - helper ctor `0x436080` builds a `0x24` event+critical-section notifier object
     - `0x4490c0` maps:
       - work type `2` -> signal `connection+0x7c`
       - work type `1` -> signal `connection+0x80`
     - current best subtype read:
       - `+0x7c` = type-2 status/connect-style completion helper
       - `+0x80` = type-1 close/terminal completion helper
     - crucial narrowing: startup auth/margin derived objects are built through `0x4417e0 -> 0x448b40(flag=0)`, so both helpers are **NULL** on that important startup path
   - the newer auth-side owner/fallback-chain pass is now also strong enough to demote that pair as the main first-send mystery:
     - auth side `0x449a70 -> owner +0x17c -> fallback 0x448a60` is now best read as **later incoming/fallback handling**, not the first outbound auth-send origin
     - owner `+0x17c` is thunk `0x41f260`, which forwards to owner current-helper `+0x10`, then jumps to helper vtable `+0x14`
     - later helper body `0x4401a0` is therefore only one **state-specific** `+0x14` target (`0x4f7890` / `0x4b512c`), not the generic owner callback itself
     - concrete `0x4401a0` only meaningfully handles later `AS_AuthReply`
       - success branch: parse via `0x43a330`, update owner `+0x80`, append owner record under `+0x684`, mirror index to owner byte `+0xcc8`, then `0x41b450(0x0b)` + `CLTLoginMediator::PostEvent(0x14)`
       - newer helper-side follow-up on that success branch is still negative for first-send purposes:
         - `0x41b450(0x0b)` selects helper `0x4f7894` (vtable `0x4b5154`)
         - helper `+0x8` / `0x43c020` prepares owner-side data and posts event `0x15`
         - it still does **not** show the missing first auth send
       - failure branch: `0x41b450(3)` + `CLTLoginMediator::PostError(0x0b)`
     - fallback `0x448a60` is only the string-backed generic logger `Got unhandled op of type %d with status %s`
     - newer later launcher-side auth send path now firmly tied down:
       - helper object `0x4f78a0` reaches send body `0x43b830`
       - auth wrapper path `0x41af60 -> auth connection +0x24 / 0x41cf30 -> +0x28 / 0x448cf0 -> 0x448a00 -> 0x449d20 -> engine +0x20`
       - packet raw code `0x35` = **`AS_GetWorldListRequest`** through the auth table
     - important negative narrowing from the same pass:
       - direct code xrefs to `0x41af60` still only show that later world-list path
       - so the missing earlier bootstrap auth send is currently **not** best modeled as just “another direct `0x41af60` caller we have not found yet”
     - current strongest earlier launcher-owned bootstrap/auth target is now the phase-2 helper chain:
       - `0x41b450(2)` selects helper `0x4f7870`
       - helper `+0x08 / 0x439210` first gates on `0x41b490()` and falls back to `0x41b450(1)` if auth is not connected yet
       - on the connected branch it gathers launcher-owned owner data via owner `+0x168`, `+0x20`, and `+0x38`, then calls `0x448050`
       - `0x448050` is currently only xref'd from `0x439210`
       - `0x448050` now also resolves more concretely than before:
         - it runs as a method on the extra owner child allocated by `0x41290` / base ctor `0x45500` and stored at owner `+0x680`
         - that phase-2 bootstrap object is size `0x11c`
         - it copies three string sources plus two 16-byte blocks into bootstrap state and stores an outbound send target at bootstrap `+0x50`
         - crucial correction: the branch at `0x44811e` is not just a loose byte-ish mode flag but the nullness of **pointer `+0xa0`** inside that bootstrap object
       - `0x448050` then branches by bootstrap helper pointer `+0xa0` into two launcher-owned outbound packet builders that both send indirectly through bootstrap `+0x50 -> +0x24`:
         - `0x447eb0` is taken when bootstrap helper pointer `+0xa0 == NULL` and sends raw code `0x06` -> strongest current **`AS_GetPublicKeyRequest`** candidate
         - `0x4474f0` is taken when bootstrap helper pointer `+0xa0 != NULL` and sends raw code `0x08` -> strongest current **`AS_AuthRequest`** candidate
         - `0x4474f0` also emits a later auxiliary raw `0x1b` packet on that same indirect path
       - newer bootstrap-object follow-up now also tightens later field meaning:
         - `0x429b0` uses bootstrap helper pointer `+0xa0 -> +0x1c` on a later incoming path
         - it writes 16-byte challenge-derived material to bootstrap `+0x85 .. +0x94`
         - `0x41470` then derives/caches a dword-ish token at bootstrap `+0x9c`
         - that same `+0x9c` value is later used by both raw `0x06` and raw `0x08` send builders
     - important channel-specific correction from the same pass:
       - margin-side wrapper traffic must be decoded through the margin table
       - raw `0x06` on `0x41af70` = **`MS_GetClientIPRequest`**, not auth `AS_GetPublicKeyRequest`
     - so the next first-send target should now move from a generic unresolved blank to the concrete phase-2 bootstrap chain above; the remaining unknown is now narrower:
       - not whether `+0xa0` matters at all
       - but what exact helper object is stored at bootstrap `+0xa0`, what exact send-target object is stored at bootstrap `+0x50`, and what exact selected-source object family is copied into the bootstrap object before that branch
   - newer env-gated diagnostic bootstrap experiment now materially changes that runtime picture:
     - with
       - `MXO_FORCE_RUNCLIENT=1`
       - `MXO_BEGIN_AUTH_CONNECTION=1`
       - `MXO_DIAGNOSTIC_SEND_AS_GETPUBLICKEY=1`
       - `MXO_DIAGNOSTIC_AUTH_LAUNCHER_VERSION=76005`
       - `MXO_DIAGNOSTIC_AUTH_CUR_PUBLIC_KEY_ID=0`
     - the launcher-side scaffold successfully sends a diagnostic raw `0x06` packet and then receives a real auth type-3 reply:
       - send log:
         - `auth bootstrap experiment rawCode=0x06 request='AS_GetPublicKeyRequest' launcherVersion=76005 currentPublicKeyId=0 byteCount=10 -> sendResult=0x00000001`
       - receive logs:
         - `received-bytes label='AuthConnection' total=408 preview=81 96 07 ... 04 00 00 00 12`
         - `auth receive framing payloadLength=406 headerBytes=2 rawCode=0x07 likelyMessage='AS_GetPublicKeyReply'`
     - important restraint:
       - this is a **diagnostic bootstrap experiment**, not faithful original-equivalent proof of the full field-population path yet
       - `76005` is still only a practical guessed launcher-version value from current `7.6005`
   - so the next runtime/auth task has now shifted one step forward:
     - stop treating the first auth bootstrap exchange as purely hypothetical
     - next target is to parse / model the live `0x07` / `AS_GetPublicKeyReply` result strongly enough to drive the next launcher-owned bootstrap send candidate `0x08` / `AS_AuthRequest`
   - keep this coupled to more faithful work-item/context modeling rather than feeding arbitrary synthetic queue items forever
