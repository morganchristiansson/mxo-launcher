# mxowrap.dll Initialization Code Flow

## Call Stack During PROCESS_ATTACH

```
DllMain (0x10020650)
  └─> DLL_PROCESS_ATTACH handler (0x1002079c)
      └─> LockModule (0x1002003b)
      └─> Version check (inline)
      └─> CriticalInit (0x1001f338)
          └─> GetProcessHandle (0x1001f7c8)
          └─> TLS_Init (0x1001ef88)
              └─> GetTLSIndex (0x1001f8cc)
              └─> ProcessTLSCallbacks
```

## Detailed Code Analysis

### DllMain Entry (0x10020650)

```asm
; Standard prologue
push    %ebx                    ; Save registers
push    %esi
push    %edi

; Load delay-load notify hook or use default
mov     0x10071898,%edi        ; __pfnDliNotifyHook2
test    %edi,%edi
jne     use_custom_hook
mov     $0x1002664f,%edi       ; Default helper

use_custom_hook:
; Get fdwReason parameter
mov     0x14(%esp),%eax        ; fdwReason is 2nd parameter

; Dispatch based on reason
sub     $0x0,%eax              ; case 0: DLL_PROCESS_DETACH
je      handle_detach

sub     $0x1,%eax              ; case 1: DLL_PROCESS_ATTACH
je      handle_attach          ; ← THIS PATH

sub     $0x1,%eax              ; case 2: DLL_THREAD_ATTACH
je      handle_thread_attach

sub     $0x1,%eax              ; case 3: DLL_THREAD_DETACH
je      handle_thread_detach

; default: call with original params
jmp     call_default_handler
```

### DLL_PROCESS_ATTACH Handler (0x1002079c)

```asm
handle_attach:
; Lock the module (increment reference count)
push    $0x1
call    LockModule             ; 0x1002003b
add     $0x4,%esp

; Get PEB for version check
mov     %fs:0x18,%eax          ; TEB
mov     0x30(%eax),%ecx        ; PEB

; Build version number from PEB fields
; Version = (Major << 48) | (Minor << 32) | (Build << 16) | Build
movzwl  0xa4(%ecx),%eax        ; PEB->OSMajorVersion
... (version assembly) ...

; Check if Windows < Vista (Major < 6)
cmp     $0x6,%esi              ; Major version
ja      skip_version_init      ; > 6, skip
jb      do_version_init        ; < 6, do init
test    %ebx,%ebx              ; == 6, check minor
jae     skip_version_init

do_version_init:
; Check initialization state
mov     0x10069634,%ecx        ; g_initState
test    %ecx,%ecx
jne     check_tls

; Set init state to 2
push    $0x2
pop     %ecx
mov     %ecx,0x10069634

check_tls:
; Check TLS index
mov     0x1006a0a4,%eax        ; g_tlsIndex
mov     %eax,0x10069630
test    %eax,%eax
jne     skip_init

; Check init state is 2
cmp     $0x2,%ecx
jne     skip_init

; Call critical initialization
call    CriticalInit           ; 0x1001f338
test    %al,%al
jne     skip_init              ; If TRUE, continue

; Return FALSE - INITIALIZATION FAILED!
xor     %eax,%eax
jmp     exit_dllmain

skip_init:
; ... continue with normal processing ...
```

### CriticalInit Function (0x1001f338)

```asm
CriticalInit:
sub     $0x38,%esp             ; Allocate locals

; Check if already initialized
cmpl    $0x0,0x1006a0a4        ; g_processHandle
jne     return_true            ; Already done

; Get process handle
call    GetProcessHandle       ; 0x1001f7c8
mov     %fs:0x18,%ecx          ; TEB
inc     %eax
mov     %eax,0x1006a0a4        ; Store process handle

; Initialize TLS
call    TLS_Init               ; 0x1001ef88
test    %al,%al
je      return_false           ; TLS init failed!

; ... more initialization ...
call    0x100202cf             ; Load modules / setup hooks

; Check result
test    %eax,%eax
je      return_false

return_true:
mov     $0x1,%al
add     $0x38,%esp
ret

return_false:
xor     %eax,%eax
add     $0x38,%esp
ret
```

### TLS_Init Function (0x1001ef88)

```asm
TLS_Init:
sub     $0xc,%esp
push    %ebx
push    %edi

; Get TLS callback array
mov     0x100607c4,%edi        ; __tls_callback_array_size
mov     %ecx,%ebx              ; Thread ID (or 0 for current)
sub     0x100607c0,%edi        ; __tls_callback_array_start
mov     %edi,0x10(%esp)        ; Store array size

; If empty, return TRUE
test    %edi,%edi
jne     have_tls_callbacks
mov     $0x1,%al
jmp     tls_done

have_tls_callbacks:
; Check process handle
cmpl    $0x0,0x1006a0a4
je      skip_tls               ; Not initialized, skip

; Get current thread if needed
test    %ebx,%ebx
jne     have_thread_id
mov     %fs:0x18,%ebx          ; TEB

have_thread_id:
mov     0x2c(%ebx),%ebp        ; Thread ID

; Get TLS index for this thread
call    GetTLSIndex            ; 0x1001f8cc

; Process TLS callbacks...
; (Complex code to enumerate and call each callback)

skip_tls:
; ... additional processing ...

tls_done:
pop     %edi
pop     %ebx
add     $0xc,%esp
ret
```

## Global Variables

| Address | Name | Purpose |
|---------|------|---------|
| 0x1006a0a4 | g_processHandle | Process handle (incremented) |
| 0x10069630 | g_tlsIndexCopy | Copy of TLS index |
| 0x10069634 | g_initState | Initialization state (0, 1, 2) |
| 0x10071898 | __pfnDliNotifyHook2 | Delay-load notify hook |
| 0x100607c0 | __tls_callback_array_start | TLS callbacks start |
| 0x100607c4 | __tls_callback_array_end | TLS callbacks end |

## TLS Callback Array Structure

From the `.tls` section (RVA 0x607c0):

```
Offset      Value       Meaning
0x607c0:    0x10056490  Callback 1 address
0x607c4:    0x10056ce8  Callback 2 address (or end marker?)
```

TLS callbacks have the signature:
```c
void NTAPI TlsCallback(PVOID hModule, DWORD dwReason, PVOID pvContext);
```

## Key Findings

1. **Multiple initialization stages**:
   - Stage 0: Not initialized
   - Stage 1: In progress (not seen in code)
   - Stage 2: Ready for TLS init

2. **TLS processing is critical**:
   - Must succeed for DllMain to return TRUE
   - Involves calling user-defined callbacks
   - May fail under Wine if callbacks use unsupported features

3. **Version-specific code**:
   - Different code paths for Windows < Vista
   - May affect behavior under Wine

## Related Functions

- `LockModule` (0x1002003b) - Increments module reference count
- `GetProcessHandle` (0x1001f7c8) - Gets process handle from PEB
- `GetTLSIndex` (0x1001f8cc) - Gets TLS slot index for thread
- `ModuleEnumeration` (0x1001f7c8) - Walks PEB loader tables

---
*Generated from disassembly analysis*
