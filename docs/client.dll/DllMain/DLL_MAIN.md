# DllMain Analysis Report for client.dll

## File Information
- **File**: ../../client.dll
- **Type**: PE32 executable (DLL) (GUI) Intel 80386, for MS Windows
- **Size**: 11 MB
- **Image Base**: 0x62000000
- **Entry Point (RVA)**: 0x006a9654
- **Entry Point (VA)**: 0x626a9654
- **Compile Time**: Sun Jan 6 04:18:59 2013

## DllMain Function Overview

The DllMain entry point is located at address `0x626a9654`. The function handles the four standard DLL notification events.

## Disassembly Analysis

### Function Prologue
```asm
626a9654:  push   $0xc              ; Reserve 12 bytes for local variables
626a9656:  push   $0x628f3718       ; Push exception handler address
626a965b:  call   0x626a9ee4        ; Set up SEH (Structured Exception Handling)
626a9660:  xor    %eax,%eax
626a9662:  inc    %eax              ; eax = 1 (default return = TRUE)
626a9663:  mov    %eax,-0x1c(%ebp)  ; Store default return value
626a9666:  xor    %edi,%edi         ; edi = 0
626a9668:  mov    %edi,-0x4(%ebp)
626a966b:  mov    0xc(%ebp),%esi    ; Load fdwReason (2nd parameter)
```

### Reason Code Checking

The function checks `fdwReason` (the reason for calling DllMain) stored in `esi`:

```asm
626a966e:  cmp    %edi,%esi         ; Compare with 0 (DLL_PROCESS_DETACH)
626a9670:  jne    0x626a967e        ; Jump if not PROCESS_DETACH
626a9672:  cmp    %edi,0x62b225d8   ; Check some global flag
626a9678:  je     0x626a972a        ; Exit if flag is 0
```

## DllMain Reason Codes Handled

| Value | Constant | Description |
|-------|----------|-------------|
| 0 | DLL_PROCESS_DETACH | Process is detaching/unloading |
| 1 | DLL_PROCESS_ATTACH | Process is attaching/loading |
| 2 | DLL_THREAD_DETACH | Thread is exiting |
| 3 | DLL_THREAD_ATTACH | New thread created |

## Detailed Behavior Analysis

### DLL_PROCESS_DETACH (fdwReason = 0)
- Checks a global flag at `0x62b225d8`
- If flag is 0, skips processing and returns

### DLL_PROCESS_ATTACH (fdwReason = 1)
```asm
626a9687:  mov    0x62b26ea4,%eax   ; Load callback pointer
626a968c:  cmp    %edi,%eax
626a968e:  je     0x626a969c        ; Skip if null
626a9690:  push   0x10(%ebp)        ; Push lpvReserved
626a9693:  push   %esi              ; Push fdwReason
626a9694:  push   0x8(%ebp)         ; Push hinstDLL
626a9697:  call   *%eax             ; Call registered callback
```

### Internal Handler Calls

The DllMain calls several internal functions:

1. **Function at 0x626a9596**: Called for initialization/cleanup
2. **Function at 0x626a9ec4**: Main handler for all reason codes

```asm
626a96c0:  call   0x626a9ec4        ; Main handler function
```

### Thread Events (DLL_THREAD_ATTACH/DETACH)
```asm
626a96db:  cmp    %edi,%esi         ; Check if DLL_PROCESS_DETACH
626a96dd:  je     0x626a96e4
626a96df:  cmp    $0x3,%esi         ; Check if DLL_THREAD_ATTACH
626a96e2:  jne    0x626a970d
626a96e4:  push   %ebx
626a96e5:  push   %esi
626a96e6:  push   0x8(%ebp)
626a96e9:  call   0x626a9596        ; Call handler
```

## Key Findings

### 1. Callback Mechanism
The DllMain supports a registered callback mechanism:
- **Callback pointer location**: `0x62b26ea4`
- If set, the callback is invoked before the main handler

### 2. Global State Flag
- **Flag location**: `0x62b225d8`
- Used during DLL_PROCESS_DETACH to determine if cleanup is needed

### 3. Multiple Handler Functions
The DllMain dispatches to multiple internal handlers:
- `0x626a9596` - Initialization/cleanup handler
- `0x626a9ec4` - Main event handler
- `0x626a9ee4` - SEH setup function

### 4. Return Value
- Default return is TRUE (1), indicating success
- Returns FALSE (0) if any handler fails

## Function Epilogue
```asm
626a970d:  orl    $0xffffffff,-0x4(%ebp)   ; Mark cleanup
626a9711:  mov    -0x1c(%ebp),%eax          ; Load return value
626a9714:  jmp    0x626a9730
626a9730:  call   0x626a9f1f                ; Cleanup function
626a9735:  ret    $0xc                      ; Return and pop 12 bytes
```

## LoadLibraryW Failure Decision Tree

```
LoadLibraryW("client.dll") returns NULL
│
├─► GetLastError() = 126 (DLL_NOT_FOUND)
│   └─ One or more dependencies not found
│       ├─ MSVCR71.dll missing? → NO (confirmed present, loading as native)
│       ├─ MFC71.dll missing? → NO (confirmed present)
│       ├─ DINPUT8.dll missing? → NO (provided by Wine)
│       └─ Other dependency missing? → All dependencies verified present
│
├─► GetLastError() = 127 (PROCEDURE_NOT_FOUND)
│   └─ Import function not found in dependency
│       └─ All imports resolvable? → LIKELY YES (no import errors logged)
│
├─► GetLastError() = 1114 (DLL_INIT_FAILED) ← CONFIRMED ERROR
│   │
│   └─ DllMain returned FALSE during DLL_PROCESS_ATTACH
│       │
│       ├─► Reason 1: Callback at 0x62b26ea4 returned FALSE
│       │   └─ Callback is in BSS (uninitialized) → Should be NULL → SKIPPED
│       │
│       ├─► Reason 2: Function 0x626a9596 returned FALSE
│       │   │
│       │   ├─► Path A: malloc(128) returned NULL
│       │   │   ├─ MSVCR71.dll not loaded? → NO (confirmed loaded at 0x7c340000 as native)
│       │   │   ├─ malloc broken? → NO (test malloc(128) works from launcher)
│       │   │   ├─ IAT entry 0x868278 not resolved? → UNKNOWN (can't inspect after failed load)
│       │   │   └─ Heap not initialized for this DLL? → POSSIBLE
│       │   │
│       │   └─► Path B: Dereference of IAT 0x86851c failed
│       │       ├─ Load _initterm pointer from IAT
│       │       ├─ Dereference that pointer
│       │       └─ If NULL or invalid → Would crash, not return FALSE
│       │
│       └─► Reason 3: Function 0x626a9ec4 returned FALSE
│           └─ Analysis shows it ALWAYS returns TRUE → RULED OUT
│
└─► GetLastError() = 193 (BAD_EXE_FORMAT)
    └─ Wrong architecture → NO (PE32 i386, matches Wine)
```

## Ruled Out Causes

| Potential Cause | Status | Evidence |
|----------------|--------|----------|
| DLL not found | ❌ Ruled Out | Wine debug shows DLL loads at 0x62000000 |
| Wrong architecture | ❌ Ruled Out | PE32 i386, correct for 32-bit Wine |
| MSVCR71.dll missing | ❌ Ruled Out | Loads as native at 0x7c340000 |
| MFC71.dll missing | ❌ Ruled Out | Loads successfully |
| Wrong base address | ❌ Ruled Out | Loads at preferred 0x62000000 |
| Callback returning FALSE | ❌ Ruled Out | Callback pointer is NULL (uninitialized BSS) |
| Function 0x626a9ec4 failing | ❌ Ruled Out | Always returns TRUE |
| Import resolution failure | ❌ Likely OK | No errors logged during import resolution |

## Remaining Suspects

| Potential Cause | Confidence | Notes |
|----------------|------------|-------|
| **malloc(128) failing in DllMain context** | HIGH | Only code path that returns FALSE for PROCESS_ATTACH |
| IAT not properly resolved for malloc | MEDIUM | Can't verify after failed load |
| Heap initialization race condition | MEDIUM | MSVCR71 may need per-DLL heap init |
| Wine incompatibility with MSVCR71 | MEDIUM | Native DLL behavior may differ under Wine |

## CRITICAL: Why LoadLibraryW Fails

### The Failure Path

During `DLL_PROCESS_ATTACH` (fdwReason=1), DllMain does the following:

```asm
626a95bf:  push   $0x80              ; Push 128 bytes
626a95c4:  call   *0x62868278        ; Call malloc(128)
626a95ca:  test   %eax,%eax          ; Check return value
626a95cc:  pop    %ecx
626a95cd:  mov    %eax,0x62b26eac    ; Store result in global
626a95d2:  jne    0x626a95d8         ; Continue if malloc succeeded
626a95d4:  xor    %eax,%eax          ; return FALSE (0)
626a95d6:  jmp    0x626a9651         ; EXIT - LoadLibrary fails!
```

**If `malloc(128)` returns NULL, DllMain returns FALSE and LoadLibraryW fails!**

### What is at 0x62868278?

This is the Import Address Table (IAT) entry for `malloc` from **MSVCR71.dll** (Visual C++ 7.1 Runtime):

| IAT Address | DLL | Function |
|-------------|-----|----------|
| 0x62868278 | MSVCR71.dll | malloc |
| 0x6286827c | MSVCR71.dll | free |

### Client.dll Dependencies (CRITICAL)

`client.dll` requires these DLLs to be present and loadable:

**Required for DllMain to succeed:**
- **MSVCR71.dll** - Visual C++ 7.1 Runtime (provides malloc, free, etc.)
- **KERNEL32.dll** - Windows kernel (always present)
- **USER32.dll** - Windows user interface

**Additional dependencies:**
- binkw32.dll - Bink video player
- DINPUT8.dll - DirectX 8 Input
- dllMediaPlayer.dll - Custom media player
- dllWebBrowser.dll - Custom web browser
- pythonMXO.dll - Python scripting
- r3d9.dll - Renderware 3D engine
- mxowrap.dll - Custom wrapper
- WININET.dll, WS2_32.dll, WSOCK32.dll - Networking

### Why LoadLibraryW("client.dll") Returns NULL

**Most likely cause: MSVCR71.dll is missing or not in PATH**

The Windows loader resolves imports BEFORE calling DllMain. If MSVCR71.dll:
1. **Is not found** → LoadLibraryW fails immediately (loader error)
2. **Is found but incompatible** → malloc call might crash or return NULL
3. **Loads successfully** → malloc(128) should work

### Solution

**The launcher must:**
1. Run from the game directory (`~/MxO_7.6005/`)
2. Pre-load MSVCR71.dll before loading client.dll
3. Ensure all dependencies are present

**Test the fix:**
```bash
cd ~/MxO_7.6005
wine ../path/to/launcher.exe
```

### What Error Code to Expect

| Error | Meaning | Cause |
|-------|---------|-------|
| 126 | DLL not found | MSVCR71.dll or other dependency missing |
| 127 | Procedure not found | Import function not found |
| **1114** | **DllMain returned FALSE** | **malloc(128) failed** |
| 193 | Bad EXE format | Wrong architecture (32/64-bit mismatch) |

**If you get error 1114**, it means:
- All DLLs were found and loaded
- DllMain was called
- But something inside DllMain failed

### Diagnostic Findings

From testing, we found:
1. ✅ client.dll loads at its preferred base address (0x62000000)
2. ✅ MSVCR71.dll loads successfully
3. ✅ `malloc(128)` works when called directly from launcher code
4. ❌ DllMain still returns FALSE

### Possible Causes

**Before malloc is called**, the code does this suspicious operation:
```asm
626a95af:  mov    0x6286851c,%ecx   ; Load pointer from IAT
626a95b5:  mov    (%ecx),%ecx       ; Dereference it
626a95b7:  mov    %ecx,0x62b26ea0   ; Store in global
```

This loads a value from IAT entry `0x86851c` and dereferences it. The import appears to be `_initterm` from MSVCR71.dll.

**If this dereference fails**, the code would crash or read garbage before even reaching malloc.

### Next Steps to Debug

1. Check if Wine is using the correct MSVCR71.dll (game's native vs Wine's built-in)
2. Verify IAT entry `0x868278` (malloc) is properly resolved when client.dll loads
3. Consider if C runtime needs special initialization

### Wine-Specific Considerations

Wine has both:
- Native MSVCR71.dll in game directory: `~/MxO_7.6005/msvcr71.dll` (348KB, Dec 2004)
- Wine's built-in MSVCR71.dll in `~/.wine/drive_c/windows/system32/`

**IMPORTANT**: Set DLL overrides to use native MSVCR71.dll:
```bash
wine reg add "HKCU\\Software\\Wine\\DllOverrides" /v "msvcr71" /t REG_SZ /d "native" /f
wine reg add "HKCU\\Software\\Wine\\DllOverrides" /v "mfc71" /t REG_SZ /d "native" /f
```

### Confirmed Error

Wine debug output shows:
```
LdrLoadDll() retval=c0000142  (STATUS_DLL_INIT_FAILED)
RtlNtStatusToDosError(c0000142) -> retval=0000045a (error 1114)
```

This confirms DllMain is returning FALSE.

### Remaining Questions

1. Why does `malloc(128)` work when called from launcher but (allegedly) fails in DllMain?
2. Is there something in the Windows environment that Wine doesn't fully implement?
3. Does the original MxO launcher do additional initialization before loading client.dll?

### Next Steps

1. Try running the original MxO launcher under Wine to see if it works
2. Check if there are any Windows-specific features required that Wine lacks
3. Consider using a Windows VM for testing instead of Wine

---

## Export/Import Analysis

### Original Launcher (launcher.exe)

**Exports:**
- `SetMasterDatabase` (ordinal 1)

**References to client.dll:**
- `client.dll` (string reference)
- `InitClientDLL` (GetProcAddress)
- `RunClientDLL` (GetProcAddress)
- `SetMasterDatabase` (string reference)

**Dependencies:**
- WINMM.dll
- MFC71.DLL
- MSVCR71.dll
- KERNEL32.dll
- USER32.dll
- GDI32.dll
- ADVAPI32.dll
- SHELL32.dll
- COMCTL32.dll
- ole32.dll
- OLEAUT32.dll
- mxowrap.dll
- VERSION.dll
- WS2_32.dll

### client.dll Exports

| Ordinal | Name |
|---------|------|
| 1 | ErrorClientDLL |
| 2 | InitClientDLL |
| 3 | RunClientDLL |
| 4 | SetMasterDatabase |
| 5 | TermClientDLL |

### Launch Sequence

The original launcher:
1. Exports `SetMasterDatabase` for client.dll to call back
2. Loads `client.dll` via LoadLibrary
3. Calls `InitClientDLL` to initialize
4. Calls `RunClientDLL` to start the game loop
5. Calls `TermClientDLL` on exit

### SetMasterDatabase Relationship

Both launcher.exe and client.dll export `SetMasterDatabase`:
- **launcher.exe exports it**: So client.dll can call back into the launcher process
- **client.dll exports it**: Possibly a passthrough or separate implementation

**Critical**: client.dll may require the loading process to export `SetMasterDatabase` **before** DllMain runs. This is unusual because:
1. The process must have the export available when client.dll is loaded
2. DllMain runs before GetProcAddress can be called
3. The IAT for this import would need to be resolved during load

### Our Launcher Implementation

Our `resurrections.exe` exports `SetMasterDatabase`:
```cpp
extern "C" __declspec(dllexport) void* SetMasterDatabase(void* db) {
    std::cout << "SetMasterDatabase called with: " << db << "\n";
    return db;
}
```

However, we never see this function called, which suggests client.dll:
1. Never gets far enough in initialization to call it, OR
2. Imports it from a specific location we haven't configured correctly

### Initialization Sequence (DLL_PROCESS_ATTACH)

```
1. malloc(128) is called
   └─ If NULL → Return FALSE → LoadLibrary fails
2. If malloc succeeded:
   └─ Zero out the allocated memory
   └─ InitializeCriticalSection at 0x626a9e36
   └─ Register atexit handler at 0x626a9288
   └─ Call some setup at 0x626a9e30
   └─ Increment reference count at 0x62b225d8
   └─ Return TRUE
```

### DLL_PROCESS_DETACH Sequence

When `fdwReason=0` and reference count > 0:
```
1. Decrement reference count
2. Call cleanup functions in reverse order (via function table)
3. free() the allocated memory
4. Return TRUE
```

---
*Analysis generated from disassembly of PE32 DLL*
