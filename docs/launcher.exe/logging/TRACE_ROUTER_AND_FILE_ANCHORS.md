# launcher.exe trace router and source-file anchors

## Purpose

Document the small logging/trace-routing cluster around `0x414be0` / `0x414e20`, and record why its file/line metadata is useful for translation-unit anchoring during static RE.

## Confirmed functions

- `0x4147d0` `LogRouter_GetOrCreateTlsMessageBuffer`
- `0x414be0` `LogRouter_FormatAndDispatchSinkMessageV`
- `0x414de0` `LogRouter_DispatchMappedSourceLocMessageV`
- `0x414e20` `LogRouter_DispatchMappedSourceLocMessageIfEnabled`
- `0x414ed0` `LogRouter_SetTlsSourceLoc`
- `0x414f10` `LogRouter_GetTlsSourceFile`
- `0x414f40` `LogRouter_GetTlsSourceLine`
- `0x414f70` `LogRouter_FprintfCompatUsingTlsSourceLoc`

## What `0x414e20` does

`0x414e20` is **not** the main TLS manager.
It is a small filtered variadic logging wrapper.

Recovered behavior:

1. compute a sink id from a 9-byte routing table at `this + 4`
   - `sinkId = *(byte *)(this + 4 + param_2 * 9 + param_3)`
2. if `sinkId == 0`, return immediately
3. if the sink enable byte at `this + 0xf7 + sinkId` is zero, return immediately
4. otherwise forward to `0x414be0`

That deeper callee performs the expensive work:

- obtains a per-thread TLS block
- formats the message into a `0x408`-byte TLS allocation
- optionally prefixes `sourceFile(sourceLine) : `
- optionally prefixes another `%s: ` label string
- dispatches the final text to one of several sinks

One now-better-understood sink is sink id `1`, which publishes into global console variables rather than directly calling a Win32 output API:

- `gConsoleColorVar` (`CConsoleInt`, name `"ConsoleColor"`)
- `gConsoleTextVar` (`CConsoleString`, name `"ConsoleText"`)

## TLS layout used by this logger

`0x4147d0` / `0x414f10` / `0x414f40` show the logger keeps a per-thread block whose observed layout is:

- `+0x000 .. +0x3ff` formatted message buffer
- `+0x400` `char *` source file pointer
- `+0x404` source line number

So TLS is present here because the logger wants thread-local formatting state and optional per-thread source-location metadata.
That is different from `0x414e20` itself "being a TLS function".

## Why the file/line metadata matters

The file-path argument is strong evidence that many callsites are produced by original source-level logging/assert macros.
Two observed patterns:

### 1. Direct source-file literal passed at the callsite

Example: `MessageBoxInternal`

- passes `0x004ac0a0` = `\matrixstaging\game\src\launcher\launcher.cpp`
- passes line `0x00d2`
- calls `0x414e20`

### 2. Source-file/line staged in TLS, then consumed later

Example: `CLTEvilBlockingLoginObserver::WaitForEvent`

- writes `\matrixstaging\game\src\libltclientlogin\loginmediator.cpp` to TLS `+0x400`
- writes line `0x486` / `0x48b` to TLS `+0x404`
- this staging can happen either inline or via `LogRouter_SetTlsSourceLoc` (`0x414ed0`)
- later logs through `LogRouter_FprintfCompatUsingTlsSourceLoc` (`0x414f70`), which uses `0x414f10` / `0x414f40`
- that wrapper then forwards to `0x414de0`

## RE value: yes, this is useful for placing code to filenames

This logging path is a good **translation-unit anchor source**.

It can be used to:

- group named and unnamed functions by original source file
- distinguish `launcher.cpp` code from `loginmediator.cpp`, `launchpad.cpp`, `ltthreadperclienttcpengine.cpp`, etc.
- prioritize renaming undocumented functions that already sit next to a recovered source-file anchor

## Important limits

Do **not** overstate the result.
This usually gives:

- the original source file / translation unit
- sometimes the original line number for the log site

It does **not** automatically prove:

- the exact original function name
- that every block in the function came from that file without inlining / reuse
- that the logging helper itself belongs to the same gameplay/login subsystem as its callers

In particular, the router helpers around `0x414be0` are shared infrastructure, not `loginmediator.cpp`-specific logic.

## Useful workflow

For broad anchor harvesting, use:

```text
mcp({ tool: "ghidra_batch_string_anchor_report", args: '{"program": "launcher.exe", "pattern": ".cpp"}' })
```

That report already shows source-file anchors such as:

- `launcher.cpp` -> `MessageBoxInternal`
- `loginmediator.cpp` -> `WaitForEvent`, `OnLoginEvent`, `OnLoginError`, `CLTLoginMediator_PostEvent`, `CLTLoginMediator_PostError`, `CLTLoginMediator_PersistCharactersIni`, `CLTLoginObserver_PassThrough_OnLoginEvent`, `CLTLoginObserver_PassThrough_OnLoginError`
- `loginstate.cpp` -> `CLTLoginState_AuthenticatePending_AuthMessageDispatch`, `CLTLoginState_WorldListPending_AuthMessageDispatch`, `GetGOBFileGUID`
- `launchpad.cpp` -> `LaunchPadClient_*`
- `ltresult.cpp` -> `CResultNameArrayItem_GetResultName`
- `pcdumpstack.cpp` -> `DumpStack`, `DumpRegisters`
- `pcprocess.cpp` -> `GetCurrentAppPath`, `CLTProcess_Open`, `CLTProcess_Start`
- `pcgetappversion.cpp` -> `GetAppVersion`
- `pccrashdump.cpp` -> `GetDbgHelpDllVersion`, `GenerateCrashDumpForProcess`
- `pcsysteminfo.cpp` -> `CLTSystemInfo_CalcPhysicalProcCount`
- `pcsocket.cpp` -> `CLTSocketLayer_Init`
- `pcdns.cpp` -> `DNSResolveNameToIPList`
- `ilttcpengine.cpp` -> `CLTBaseThreadPerClientTCPEngine_EnqueueCompletedOperation`, `CLTBaseThreadPerClientTCPEngine_RunCompletedOperationQueue`, `ILTTCPEngine_StopQueueThreads`
- `ltipaddresslist.cpp` -> `CLTIPAddressList_Reinit`
- `lttcpconnection.cpp` -> `CLTIPSocket_StaticAllocateSocket`, `CLTTCPConnection_OnReceive`
- `ltthreadperclienttcpengine.cpp` -> `CLTThreadPerClientTCPEngine__Close`, `CLTThreadPerClientTCPEngine__SendBuffer`, `CLTThreadPerClientTCPEngine__MonitorPort`, `CLTThreadPerClientTCPEngine__CleanupConnection`, `CLTThreadPerClientTCPEngine__Connect`, `CLTThreadPerClientTCPEngine__UDPMonitorPort`, `CLTThreadPerClientTCPEngine_dtor`, `CLTThreadPerClientTCPEngine_AcceptThread_Run`, `CLTThreadPerClientTCPEngine_WorkerThread_Run`
- `messageconnection.cpp` -> `CMessageConnection_LogUnhandledOperation`, `CMessageConnection_OnOperationCompleted`, `CMessageConnection_SendPacket`
- `sessionkeyencryption.cpp` -> `CPacketDecryptor_DecryptPacket`
- `variablelengthprefixedtcpstreamparser.cpp` -> `CVariableLengthPrefixedTCPStreamParser_Parse`
- `filters.cpp` -> `StreamTransformationFilter_ctor`, `StreamTransformationFilter_LastPut`
- `ltresourcemgr.cpp` -> `LTResourceMgr_FindOrOpenCachedFileEntry`

Current status note:

- after the latest naming pass, the current `.cpp` anchor report for `launcher.exe` now shows **zero remaining undocumented functions** among the currently discovered source-file-anchored functions
- that does **not** mean the whole program is fully named
- it does mean this source-file-anchor workflow is now paying off and can be reused as new anchors/classes are recovered

## Confidence

High for:

- `0x414e20` being a filtered front-end for the shared trace/log router
- TLS use belonging to message-buffer/source-location storage
- source-file strings from this path being useful TU anchors for static RE
