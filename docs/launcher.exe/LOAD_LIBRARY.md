# DLL Loading Mechanism Analysis

## Overview
This document analyzes how launcher.exe loads client.dll and handles the DLL search path so that client.dll can find `dllWebBrowser.dll` in the `p_dlls` subfolder.

**Analysis Date**: Phase 3.4
**Status**: COMPLETED
**Binaries**: launcher.exe (5.3 MB), client.dll (11 MB)

---

## 1. Key Discovery: DLL Deployment, Not Search Path Modification

### 1.1 The Mechanism

**CRITICAL FINDING**: Launcher.exe does **NOT** modify the DLL search path using `SetDllDirectory` or `AddDllDirectory`. Instead, it uses a **DLL deployment mechanism** that copies DLLs from the `p_dlls/` subfolder to the game root directory.

### 1.2 Evidence

No calls to the following Windows APIs were found:
- `SetDllDirectoryA/W` - NOT USED
- `AddDllDirectory` - NOT USED  
- `SetEnvironmentVariableA/W` for PATH - NOT USED
- `LoadLibraryExA/W` with special flags - NOT USED

---

## 2. DLL Deployment Function

### 2.1 Function: `fcn.0040a300` (DLL Deployment)

**Purpose**: Copies DLLs from `p_dlls/` subfolder to game root directory if needed.

**Function Signature**:
```c
BOOL DeployDLL(const char* source_path, const char* dest_filename);
// source_path: e.g., "p_dlls/dllWebBrowser.dll"
// dest_filename: e.g., "dllWebBrowser.dll"
```

**Algorithm**:
1. Check if source file exists using `_stat()`
2. Check if destination file exists using `_stat()`
3. Compare file timestamps and sizes
4. If files differ, copy source to destination using `CopyFileA()`
5. Return TRUE if successful, FALSE otherwise

**Assembly Analysis**:
```assembly
fcn.0040a300:
    ; Check if source file (arg_8h) exists
    push    eax                     ; stat buffer
    push    ebx                     ; source path
    call    _stat
    test    eax, eax
    jne     .exit_fail              ; Source doesn't exist
    
    ; Check if destination file (arg_ch) exists
    push    ecx                     ; stat buffer
    push    esi                     ; dest path
    call    _stat
    test    eax, eax
    jne     .copy_file              ; Dest doesn't exist, need to copy
    
    ; Compare file modification times
    mov     edx, [var_10h]          ; Source mtime
    cmp     edx, [var_34h]          ; Dest mtime
    jne     .copy_file              ; Different times
    
    ; Compare file sizes
    mov     eax, [var_8h]           ; Source size
    cmp     eax, [var_2ch]          ; Dest size
    je      .skip_copy              ; Same file, skip copy
    
.copy_file:
    ; Remove read-only attribute if set
    push    esi
    call    GetFileAttributesA
    test    al, 1
    je      .do_copy
    and     eax, 0xfffffffe
    push    eax
    push    esi
    call    SetFileAttributesA
    
.do_copy:
    ; Copy file from source to destination
    push    0                       ; Fail if exists = FALSE
    push    esi                     ; Destination filename
    push    ebx                     ; Source path
    call    CopyFileA
    
.skip_copy:
    ; Return success
    mov     al, 1
    ret
```

---

## 3. DLLs Deployed from p_dlls/

### 3.1 DLL List

The following DLLs are deployed from `p_dlls/` to the game root:

| Source Path | Destination | Purpose |
|-------------|-------------|---------|
| `p_dlls/dllWebBrowser.dll` | `dllWebBrowser.dll` | Web browser component |
| `p_dlls/patchctl.dll` | `patchctl.dll` | Patch control |
| `p_dlls/xpatcher.dll` | `xpatcher.dll` | Extended patcher |

### 3.2 String Evidence

Found in launcher.exe strings:
```
0x4ac290: "p_dlls/dllWebBrowser.dll"
0x4ac2ac: "dllWebBrowser.dll"
0x4ac248: "p_dlls/patchctl.dll"
0x4ac25c: "patchctl.dll"
0x4ac26c: "p_dlls/xpatcher.dll"
0x4ac280: "xpatcher.dll"
```

---

## 4. Initialization Sequence

### 4.1 Function: `fcn.0040a880` (DLL Deployment Handler)

This function handles the deployment of all p_dlls DLLs:

```assembly
fcn.0040a880:
    ; Get current working directory
    push    0x104                   ; MAX_PATH = 260
    lea     eax, [var_108h]
    push    eax
    call    _getcwd
    
    ; Get command line arguments
    call    __p___argv
    mov     ecx, [eax]
    mov     eax, [ecx]              ; argv[0] = program path
    
    ; Deploy dllWebBrowser.dll
    push    "dllWebBrowser.dll"
    push    "p_dlls/dllWebBrowser.dll"
    call    fcn.0040a300            ; DeployDLL
    
    ; Deploy xpatcher.dll
    push    "xpatcher.dll"
    push    "p_dlls/xpatcher.dll"
    call    fcn.0040a300
    
    ; Deploy patchctl.dll
    push    "patchctl.dll"
    push    "p_dlls/patchctl.dll"
    call    fcn.0040a300
    
    ret
```

### 4.2 Complete Initialization Flow

```
launcher.exe startup
    │
    ├── fcn.0040a880: Deploy p_dlls DLLs
    │   ├── Deploy "p_dlls/dllWebBrowser.dll" → "dllWebBrowser.dll"
    │   ├── Deploy "p_dlls/xpatcher.dll" → "xpatcher.dll"
    │   └── Deploy "p_dlls/patchctl.dll" → "patchctl.dll"
    │
    ├── fcn.0040a420: Load client.dll
    │   └── LoadLibraryA("client.dll")
    │       └── client.dll can now find dllWebBrowser.dll in game root
    │
    └── Continue initialization...
```

### 4.3 Code Location Analysis

| Address | Function | Purpose |
|---------|----------|---------|
| 0x40b68a | Call to fcn.0040a880 | Deploy p_dlls DLLs |
| 0x40b6e2-0x40b6ec | Direct call to fcn.0040a300 | Deploy dllWebBrowser.dll specifically |
| 0x40b7bc | Call to fcn.0040a420 | Load client.dll |

---

## 5. How client.dll Finds dllWebBrowser.dll

### 5.1 Windows DLL Search Order

When `client.dll` is loaded via `LoadLibraryA("client.dll")`, Windows uses the standard DLL search order:

1. **Application directory** (directory containing launcher.exe)
2. System directories (System32, etc.)
3. Windows directory
4. Current directory
5. PATH environment variable directories

### 5.2 The Solution

Since launcher.exe:
1. Copies `p_dlls/dllWebBrowser.dll` to `dllWebBrowser.dll` in the game root
2. Then loads `client.dll` from the same directory

When `client.dll` needs `dllWebBrowser.dll`, Windows finds it in the **application directory** (game root), which is the first search location.

### 5.3 Why This Works

```
Game Root Directory (Application Directory)
├── launcher.exe
├── client.dll              (loaded by launcher.exe)
├── dllWebBrowser.dll       (copied from p_dlls/)
├── patchctl.dll            (copied from p_dlls/)
├── xpatcher.dll            (copied from p_dlls/)
└── p_dlls/
    ├── dllWebBrowser.dll   (source)
    ├── patchctl.dll        (source)
    └── xpatcher.dll        (source)
```

When `client.dll` calls `LoadLibrary("dllWebBrowser.dll")`, Windows:
1. Looks in application directory (game root)
2. Finds `dllWebBrowser.dll` there
3. Loads it successfully

---

## 6. client.dll Loading Analysis

### 6.1 Function: `fcn.0040a420` (Load client.dll)

```assembly
fcn.0040a420:
    ; Load client.dll
    push    "client.dll"            ; 0x4ac160
    call    LoadLibraryA
    mov     [0x4d2c50], eax         ; Store module handle
    test    eax, eax
    jne     .success
    
    ; Handle load failure
    call    GetLastError
    ; ... error handling code ...
    
.success:
    ret
```

### 6.2 Module Handle Storage

- **Global Variable**: `0x4d2c50`
- **Type**: `HMODULE`
- **Purpose**: Store client.dll module handle for later use

---

## 7. Comparison: Expected vs Actual Implementation

### 7.1 Expected (Search Path Modification)

```c
// Expected approach (NOT USED)
SetDllDirectory("p_dlls");          // Add p_dlls to search path
LoadLibraryA("client.dll");          // client.dll can now find DLLs in p_dlls
```

### 7.2 Actual (DLL Deployment)

```c
// Actual approach (USED)
CopyFileA("p_dlls/dllWebBrowser.dll", "dllWebBrowser.dll", FALSE);
CopyFileA("p_dlls/patchctl.dll", "patchctl.dll", FALSE);
CopyFileA("p_dlls/xpatcher.dll", "xpatcher.dll", FALSE);
LoadLibraryA("client.dll");          // client.dll finds DLLs in game root
```

### 7.3 Advantages of Deployment Approach

1. **Simplicity**: No need to manage DLL search paths
2. **Compatibility**: Works on all Windows versions
3. **Isolation**: Each game installation has its own DLL copies
4. **Updates**: Easy to update individual DLLs by replacing in p_dlls

### 7.4 Disadvantages

1. **Disk Space**: Duplicate DLLs (in p_dlls and game root)
2. **Consistency**: Need to ensure deployed DLLs match source
3. **Maintenance**: More complex update process

---

## 8. Technical Details

### 8.1 File Comparison Logic

The deployment function compares:
- **Modification time** (`st_mtime` from `_stat` structure)
- **File size** (`st_size` from `_stat` structure)

If either differs, the file is re-copied.

### 8.2 File Attributes Handling

Before copying, the function:
1. Checks if destination has read-only attribute
2. Removes read-only if present using `SetFileAttributesA`
3. Then performs the copy

### 8.3 Error Handling

- Returns `FALSE` (0) if source file doesn't exist
- Returns `TRUE` (1) if files are identical (no copy needed)
- Returns `TRUE` (1) if copy successful
- Returns `FALSE` (0) if copy failed

---

## 9. Summary

### 9.1 Key Findings

| Aspect | Finding |
|--------|---------|
| **DLL Search Path** | NOT modified |
| **SetDllDirectory** | NOT used |
| **AddDllDirectory** | NOT used |
| **Mechanism** | DLL deployment via file copy |
| **Source Location** | `p_dlls/` subfolder |
| **Destination** | Game root directory |
| **Function** | `fcn.0040a300` (deploy) + `fcn.0040a880` (batch deploy) |

### 9.2 DLL Loading Sequence

1. Launcher starts
2. Deploy DLLs from `p_dlls/` to game root
3. Load `client.dll` with `LoadLibraryA("client.dll")`
4. client.dll can find dependencies in game root

### 9.3 Why p_dlls Exists

The `p_dlls/` folder serves as:
- **Source repository** for patchable DLLs
- **Update staging area** for game patches
- **Organized storage** separate from main game files

---

## 10. References

- **launcher.exe**: `../../launcher.exe` (5,267,456 bytes)
- **client.dll**: `../../client.dll` (10,924,032 bytes)
- **Functions Analyzed**:
  - `fcn.0040a300`: DLL deployment function
  - `fcn.0040a880`: Batch DLL deployment
  - `fcn.0040a420`: client.dll loader
- **Tools**: radare2 (r2)

---

## 11. Cleanup Behavior - Are Deployed DLLs Deleted?

### 11.1 Investigation Results

**ANSWER: No, the deployed DLLs are NOT deleted at exit.**

### 11.2 Evidence

**FreeLibrary vs DeleteFile**:
- Launcher.exe uses `FreeLibrary` to **unload** DLLs from memory at exit
- Launcher.exe does **NOT** call `DeleteFileA` on deployed DLLs
- The only `DeleteFileA` call found deletes `options.cfg` (a temporary config file)

**Cleanup Functions Found**:

| Function | Purpose | Action |
|----------|---------|--------|
| `fcn.0040a760` | Unload client.dll | `FreeLibrary(0x4d2c50)` - module handle |
| `fcn.0040a7a0` | Unload secondary module | `FreeLibrary(0x4d2c4c)` |
| `fcn.004012e0` | Delete temp config | `DeleteFileA("options.cfg")` |

**Assembly Evidence**:
```assembly
; fcn.0040a760 - Unload client.dll
mov     eax, [0x4d2c50]      ; Load client.dll module handle
test    eax, eax
je      .skip
push    eax
call    FreeLibrary          ; Unload from memory (NOT delete from disk)
.skip:
ret
```

### 11.3 File Deletion Analysis

**What IS deleted**:
- `options.cfg` - temporary configuration file
- Location: Same directory as launcher.exe

**What is NOT deleted**:
- `dllWebBrowser.dll` - deployed from p_dlls
- `patchctl.dll` - deployed from p_dlls
- `xpatcher.dll` - deployed from p_dlls
- `client.dll` - loaded from game root

### 11.4 Why DLLs Are Not Deleted

The deployed DLLs remain on disk because:

1. **Performance**: Avoid re-copying on every launch
2. **Patch Management**: Deployed DLLs represent current patched version
3. **Update Detection**: Deployment function compares timestamps to determine if copy is needed
4. **Persistent Installation**: DLLs are part of the installed game, not temporary files

### 11.5 Deployment Persistence

The deployment function (`fcn.0040a300`) includes logic to avoid unnecessary copies:

```c
BOOL DeployDLL(const char* source, const char* dest) {
    // Check if files exist and match
    if (stat(source) == 0 && stat(dest) == 0) {
        // Compare timestamps and sizes
        if (source_mtime == dest_mtime && source_size == dest_size) {
            // Files match, skip copy
            return TRUE;
        }
    }
    // Files differ, perform copy
    return CopyFileA(source, dest, FALSE);
}
```

This ensures DLLs are only copied when:
- Destination doesn't exist
- Source is newer than destination
- File sizes differ

### 11.6 Exit Sequence

```
Game Exit
    │
    ├── fcn.0040a7a0: FreeLibrary(secondary_module)
    │
    ├── fcn.0040a760: FreeLibrary(client.dll)
    │
    ├── fcn.004012e0: DeleteFileA("options.cfg")
    │
    └── Process terminates
    
    Note: Deployed DLLs (dllWebBrowser.dll, etc.) remain on disk
```

### 11.7 Summary

| Item | Action at Exit | Persistent? |
|------|----------------|-------------|
| client.dll (in memory) | Unloaded via FreeLibrary | N/A |
| dllWebBrowser.dll | No action | **YES** - remains on disk |
| patchctl.dll | No action | **YES** - remains on disk |
| xpatcher.dll | No action | **YES** - remains on disk |
| options.cfg | Deleted via DeleteFileA | **NO** - temporary file |

---

## 12. Conclusion

Launcher.exe does **not** modify the DLL search path to enable client.dll to find dllWebBrowser.dll. Instead, it uses a **DLL deployment mechanism** that copies DLLs from the `p_dlls/` subfolder to the game root directory before loading client.dll.

This approach is simpler and more compatible across Windows versions, but requires managing duplicate DLL files. The p_dlls folder acts as a source repository for patchable game components, with the launcher responsible for deploying them to the correct location before the game starts.

**The deployed DLLs persist on disk after exit** - they are not deleted. This allows the launcher to skip deployment on subsequent launches if the files haven't changed, improving startup performance.
