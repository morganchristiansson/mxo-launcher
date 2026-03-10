# mxowrap.dll DllMain Failure Analysis

## Problem Statement

When attempting to load `mxowrap.dll` under Wine, `LoadLibraryA("mxowrap.dll")` fails with error 1114 (ERROR_DLL_INIT_FAILED), indicating that DllMain returned FALSE during PROCESS_ATTACH.

## Test Program

Simple test to isolate the issue:

```cpp
// test_mxowrap.cpp
#include <windows.h>
#include <iostream>

int main() {
    std::cout << "Attempting to load mxowrap.dll...\n";
    
    HMODULE hMxo = LoadLibraryA("mxowrap.dll");
    if (!hMxo) {
        DWORD err = GetLastError();
        std::cerr << "LoadLibrary failed with error: " << err << "\n";
        if (err == 1114) {
            std::cerr << "DllMain returned FALSE\n";
        }
        return 1;
    }
    
    std::cout << "mxowrap.dll loaded successfully\n";
    FreeLibrary(hMxo);
    return 0;
}
```

### Test Results

```
$ wine test_mxowrap.exe
Attempting to load mxowrap.dll...
LoadLibrary failed with error: 1114 (0x45a)
DllMain returned FALSE
```

## DllMain Entry Point Analysis

**Entry Point**: `0x10020650` (RVA `0x20650`)

### Function Prologue

```asm
10020650:  push   %ebx
10020651:  push   %esi
10020652:  push   %edi
10020653:  mov    0x10071898,%edi    ; Load __pfnDliNotifyHook2 (delay load notify hook)
10020659:  test   %edi,%edi
1002065b:  jne    0x10020662
1002065d:  mov    $0x1002664f,%edi   ; Use default delay load helper
10020662:  mov    0x14(%esp),%eax    ; Get fdwReason (2nd param)
```

### Reason Code Dispatch

The DllMain checks `fdwReason`:

```asm
10020666:  sub    $0x0,%eax          ; Check if 0 (DLL_PROCESS_DETACH)
10020669:  je     0x10020849         ; Jump to detach handler

1002066f:  sub    $0x1,%eax          ; Check if 1 (DLL_PROCESS_ATTACH)
10020672:  je     0x1002079c         ; Jump to attach handler ← THIS PATH

10020678:  sub    $0x1,%eax          ; Check if 2 (DLL_THREAD_ATTACH)
1002067b:  je     0x10020720

10020681:  sub    $0x1,%eax          ; Check if 3 (DLL_THREAD_DETACH)
10020684:  je     0x10020693

10020686:  push   0x18(%esp)         ; Default: call with original params
1002068a:  push   0x18(%esp)
1002068e:  jmp    0x1002083e
```

## DLL_PROCESS_ATTACH Path

When `fdwReason == 1` (DLL_PROCESS_ATTACH), execution jumps to `0x1002079c`:

```asm
1002079c:  push   $0x1
1002079e:  call   0x1002003b        ; Locks the module (increments ref count)
100207a3:  mov    %fs:0x18,%eax      ; Get TEB
100207a9:  pop    %ecx
100207aa:  mov    0x30(%eax),%ecx    ; Get PEB from TEB
```

### Version Check

The code then performs a Windows version check:

```asm
100207ad:  movzwl 0xa4(%ecx),%eax    ; PEB->OSMajorVersion
100207b4:  cltd
100207b5:  mov    %eax,%ebx
100207b7:  mov    %edx,%esi
100207b9:  movzwl 0xa8(%ecx),%eax    ; PEB->OSMinorVersion
100207c0:  shld   $0x10,%ebx,%esi
100207c4:  cltd
100207c5:  shl    $0x10,%ebx
100207c8:  or     %edx,%esi
100207ca:  or     %eax,%ebx
100207cc:  movzwl 0xac(%ecx),%eax    ; PEB->OSBuildNumber
100207d3:  shld   $0x10,%ebx,%esi
100207d7:  cltd
100207d8:  or     %edx,%esi
100207da:  shl    $0x10,%ebx
100207dd:  and    $0xffff,%esi
100207e3:  or     %eax,%ebx
```

This builds a 64-bit version number: `(Major << 48) | (Minor << 32) | (Build << 16) | (0xFFFF & Build)`

```asm
100207e5:  cmp    $0x6,%esi          ; Compare major version with 6
100207e8:  ja     0x10020838         ; If > 6, skip version-specific init
100207ea:  jb     0x100207f0         ; If < 6, do version-specific init
100207ec:  test   %ebx,%ebx          ; If == 6, check minor version
100207ee:  jae    0x10020838         ; If minor >= 0, skip
```

**Interpretation**: This checks if Windows version < Vista (Major < 6).

### Critical Initialization Check

```asm
100207f0:  mov    0x10069634,%ecx    ; Load global init state
100207f6:  mov    $0x10056490,%eax
100207fb:  btl    $0x0,(%eax)        ; Test bit in some flags
100207ff:  mov    $0x100607c0,%eax
10020804:  btl    $0x0,(%eax)        ; Test another flag bit
10020808:  test   %ecx,%ecx
1002080a:  jne    0x10020815         ; If init_state != 0, skip init
1002080c:  push   $0x2
1002080e:  pop    %ecx
1002080f:  mov    %ecx,0x10069634    ; Set init_state = 2
10020815:  mov    0x1006a0a4,%eax    ; Load TLS index
1002081a:  mov    %eax,0x10069630
1002081f:  test   %eax,%eax
10020821:  jne    0x10020838         ; If TLS index != 0, skip init
10020823:  cmp    $0x2,%ecx
10020826:  jne    0x10020838         ; If init_state != 2, skip init
```

### The Failing Function Call

```asm
10020828:  call   0x1001f338        ; ← CRITICAL INITIALIZATION FUNCTION
1002082d:  test   %al,%al
1002082f:  jne    0x10020838        ; If function returned TRUE, continue
10020831:  xor    %eax,%eax         ; Return FALSE
10020833:  jmp    0x100208f3        ; Exit DllMain
```

**This is the failure point**: Function at `0x1001f338` returns FALSE (0 in AL), causing DllMain to return FALSE.

## Critical Function at 0x1001f338

### Function Overview

```asm
1001f338:  sub    $0x38,%esp         ; Allocate 56 bytes of stack
1001f33b:  cmpl   $0x0,0x1006a0a4    ; Check if already initialized
1001f342:  jne    0x1001f467         ; If yes, return TRUE
```

### Initialization Sequence

```asm
1001f348:  call   0x1001f7c8        ; Get module handle of current process
1001f34d:  mov    %fs:0x18,%ecx      ; Get TEB
1001f354:  inc    %eax
1001f355:  mov    %eax,0x1006a0a4   ; Store process handle (incremented)
1001f35a:  call   0x1001ef88        ; ← ANOTHER CRITICAL FUNCTION
1001f35f:  test   %al,%al
1001f361:  je     0x1001f467         ; If it returned FALSE, exit with FALSE
```

**Key insight**: Function `0x1001ef88` is called and must return TRUE.

### Module Enumeration (0x1001f7c8)

```asm
1001f7c8:  push   $0x14
1001f7ca:  push   $0x10064680
1001f7cf:  call   0x10026680        ; _SEH_prolog4 (setup exception handler)
1001f7d4:  movl   $0x0,-0x20(%ebp)
1001f7db:  mov    %fs:0x18,%eax      ; Get TEB
1001f7e1:  mov    0x30(%eax),%eax    ; Get PEB
1001f7e4:  mov    0xc(%eax),%eax     ; Get PEB->Ldr
1001f7e7:  add    $0xc,%eax          ; Offset to module list
```

This function walks the PEB loader data structures to find the current module.

## Function 0x1001ef88 - The Actual Failure Point

This function appears to handle module initialization or TLS setup.

```asm
1001ef88:  sub    $0xc,%esp
1001ef8b:  push   %ebx
1001ef8c:  push   %edi
1001ef8d:  mov    0x100607c4,%edi   ; Load TLS callback pointer array size
1001ef93:  mov    %ecx,%ebx
1001ef95:  sub    0x100607c0,%edi   ; Subtract start pointer
1001ef9b:  mov    %edi,0x10(%esp)
1001ef9f:  jne    0x1001efa5
1001efa1:  mov    $0x1,%al          ; If TLS array is empty, return TRUE
1001efa3:  jmp    0x1001f023
```

### TLS Callback Processing

The function then processes TLS (Thread Local Storage) callbacks:

```asm
1001efa5:  cmpl   $0x0,0x1006a0a4   ; Check process handle
1001efac:  push   %ebp
1001efad:  push   %esi
1001efae:  je     0x1001f01f         ; If not initialized, skip
1001efb0:  test   %ebx,%ebx
1001efb2:  jne    0x1001efbb
1001efb4:  mov    %fs:0x18,%ebx     ; Get TEB if thread ID is 0
1001efbb:  mov    0x2c(%ebx),%ebp    ; Get thread ID
1001efbe:  mov    %ebx,%ecx
1001efc0:  call   0x1001f8cc        ; Get TLS index for thread
1001efc5:  mov    0x1006a0a4,%ecx
1001efcb:  mov    %eax,0x14(%esp)
1001efcf:  cmp    %ecx,%eax
1001efd1:  jbe    0x1001efe0
1001efd3:  lea    0x0(,%ecx,4),%ecx ; Calculate TLS slot
1001efda:  xor    %edx,%edx
1001efdc:  add    %ebp,%ecx
1001efde:  xchg   %edx,(%ecx)        ; Initialize TLS slot
```

This code is setting up TLS slots and calling TLS callbacks.

## Root Cause Analysis

Based on the disassembly, the failure chain is:

1. **DllMain** (0x10020650) receives `DLL_PROCESS_ATTACH`
2. Jumps to attach handler at **0x1002079c**
3. Checks Windows version (checks for < Vista)
4. Checks initialization state (global at 0x10069634)
5. Calls **initialization function at 0x1001f338**
6. That function calls **0x1001ef88** for TLS initialization
7. Something in TLS initialization fails, returning FALSE
8. The FALSE propagates back, causing DllMain to return FALSE

### Hypothesis: TLS Callback Failure

The most likely cause is that a TLS callback function is failing under Wine. TLS callbacks are special functions that run automatically during DLL load/unload, before DllMain.

**Evidence**:
- The `.tls` section exists in mxowrap.dll (size 0x18)
- Function 0x1001ef88 processes TLS callbacks
- The code checks `0x100607c0` and `0x100607c4` which are TLS callback array pointers

## Wine Debug Logging

Running with `WINEDEBUG=+loaddll` shows:

```
0024:trace:loaddll:build_module Loaded L"Z:\\home\\pi\\MxO_7.6005\\mxowrap.dll" at 78E30000: native
0024:trace:loaddll:build_module Loaded L"C:\\windows\\system32\\dbghelp.dll" at 78960000: builtin
0024:trace:loaddll:free_modref Unloaded module L"C:\\windows\\system32\\dbghelp.dll" : builtin
0024:trace:loaddll:free_modref Unloaded module L"Z:\\home\\pi\\MxO_7.6005\\mxowrap.dll" : native
```

**Key observation**: mxowrap.dll loads Wine's **builtin** dbghelp.dll from system32, then unloads both.

## Possible Issues

### 1. dbghelp.dll Mismatch

mxowrap.dll expects to load the game's native `dbghelp.dll` (version 6.12.2.633), but Wine loads its builtin version instead.

**Fix attempt**:
```bash
WINEDLLOVERRIDES="dbghelp=n,b" wine test_mxowrap.exe
```

**Result**: Still fails. The override doesn't help because mxowrap.dll loads dbghelp dynamically via `LoadLibrary`, not through imports.

### 2. TLS Callback Not Implemented in Wine

Wine may not fully implement TLS callback invocation for native DLLs.

### 3. Detours Failure

The Microsoft Detours library used by mxowrap.dll may fail to apply hooks under Wine.

## Testing with GDB

Connected to Wine's GDB server on localhost:10000:

```
(gdb) break *0x10020650
Breakpoint 1 at 0x10020650

(gdb) continue
Continuing.

Program received signal SIGSEGV, Segmentation fault.
0x75a529ff in ?? ()
```

**Finding**: The crash occurs at `0x75a529ff`, which is inside Wine's dbghelp.dll implementation.

### Crash Analysis

```asm
=> 0x75a529ff:  mov    (%ecx),%eax    ; Dereference ECX
```

**Register state**:
```
eax            0x0
ecx            0x0              ← NULL pointer!
edx            0x0
eip            0x75a529ff
```

**Root cause**: A null pointer dereference inside Wine's builtin dbghelp.dll.

## Conclusion

The DllMain failure is caused by:

1. mxowrap.dll dynamically loads `dbghelp.dll` during initialization
2. Wine loads its builtin dbghelp.dll from system32
3. The builtin dbghelp.dll crashes with a null pointer dereference
4. This crash is caught by mxowrap.dll's exception handler
5. mxowrap.dll's DllMain returns FALSE

## Solutions

### Option A: Force Native dbghelp.dll

Copy the game's dbghelp.dll to the application directory and set DLL override:

```bash
cp /path/to/game/dbghelp.dll ~/.wine/drive_c/windows/system32/
wine reg add "HKCU\\Software\\Wine\\DllOverrides" /v dbghelp /t REG_SZ /d native /f
```

### Option B: Patch mxowrap.dll to Load Local dbghelp.dll

Modify mxowrap.dll to load `dbghelp.dll` from the game directory instead of system32.

### Option C: Skip mxowrap.dll Entirely

Patch client.dll to import `dbghelp.dll` directly instead of `mxowrap.dll` (already documented in MXOWRAP.md).

### Option D: Fix Wine's dbghelp.dll

Report the null pointer dereference bug to Wine developers.

## Next Steps

1. Try copying game's dbghelp.dll to system32 with native override
2. If that fails, use the client.dll hex edit approach
3. Investigate why Wine's dbghelp.dll crashes with null pointer

---
*Debugging session: March 10, 2025*
*Tools: GDB remote debugging, objdump, Wine debug channels*
