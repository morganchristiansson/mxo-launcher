# OnPacketSend

## Overview

**Category**: Network
**Direction**: Launcher → Client
**Purpose**: Network packet send callback - invoked after packet processing/transmission
**VTable Index**: N/A (event callback)
**Byte Offset**: Object+0x70

---

## Function Signature

```c
int OnPacketSend(void* packetData, uint32_t reserved);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `void*` | packetData | Pointer to packet data buffer that was processed |
| `uint32_t` | reserved | Reserved parameter (always 0 in disassembly) |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Continue normal processing |
| `int` | Non-zero | Custom return value (stored and may affect flow) |

---

## Calling Convention

**Type**: `__cdecl` (callback)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  void* packetData
[ESP+8]  uint32_t reserved (always 0)

Registers:
EAX = return value
```

---

## Implementation Details

### Disassembly Analysis

**Location**: Function at 0x448d00+ (SendPacket implementation)

```assembly
; Extract packet metadata from connection object
8d 45 e8              lea    -0x18(%ebp),%eax
8d 73 24              lea    0x24(%ebx),%esi    ; Object+0x24 = packet metadata
50                    push   %eax
8b ce                 mov    %esi,%ecx
e8 55 21 00 00        call   0x44aff0           ; Extract metadata function

; Multiple metadata extractions
8b 4d e8              mov    -0x18(%ebp),%ecx
8b 55 ec              mov    -0x14(%ebp),%edx
8b 45 f0              mov    -0x10(%ebp),%eax
; ... more extractions ...

; Invoke callback if registered
8b 43 70              mov    0x70(%ebx),%eax    ; Get callback from Object+0x70
85 c0                 test   %eax,%eax          ; Check if NULL
74 0c                 je     0x448ef2           ; Skip if NULL
6a 00                 push   $0x0               ; Push reserved (0)
57                    push   %edi               ; Push packet data
ff d0                 call   *%eax              ; Call callback
83 c4 08              add    $0x8,%esp          ; Clean up stack
8b f0                 mov    %eax,%esi          ; Store return value
eb 05                 jmp    0x448ef7

; If no callback registered, use default value
be bb af 4a 00        mov    $0x4aafbb,%esi     ; Default return value
```

### Key Findings from Disassembly

1. **Callback Storage**: The callback function pointer is stored at offset **0x70** in the connection object
2. **Callback Invocation**: The callback is invoked with two parameters:
   - First parameter: `%edi` (packet data pointer)
   - Second parameter: `0` (reserved/unused)
3. **Return Value Handling**: The callback's return value is stored in `%esi` and affects subsequent flow
4. **Metadata Extraction**: Before the callback, multiple calls to function `0x44aff0` extract packet metadata from offset `0x24` of the connection object
5. **Conditional Execution**: The callback is only invoked if non-NULL (test and je instruction sequence)

---

## Data Structures

### Connection Object Structure (Partial)

```c
struct ConnectionObject {
    void* vtable;                    // +0x00: VTable pointer
    // ...
    char sourceFile[36];             // +0x00: Source file name "rc\libltmessaging\messageconnection.cpp"
    PacketMetadata* packetMeta;      // +0x24: Packet metadata pointer
    // ...
    void* callbackSend;              // +0x70: OnPacketSend callback pointer
    void* callback74;                // +0x74: Another callback pointer
    uint8_t flag78;                  // +0x78: Flag byte
    void* callback7c;                // +0x7c: Another callback pointer
    void* callback80;                // +0x80: Another callback pointer
};
```

### Packet Metadata Structure

Based on the disassembly, metadata is extracted at multiple offsets:
- Multiple 32-bit values extracted for logging
- Used for diagnostic message formatting

---

## Usage

### Registration

Register the OnPacketSend callback by setting the function pointer at offset 0x70:

```c
// Method 1: Direct assignment
ConnectionObject* conn = GetConnection();
conn->callbackSend = MyOnPacketSend;  // Set at offset 0x70

// Method 2: Through API (if available)
// Registration method not yet identified in disassembly
```

### Assembly Pattern

```assembly
; SendPacket function flow
SendPacket:
    push ebp
    mov ebp, esp
    
    ; Extract packet metadata
    lea eax, [metadata_buffer]
    lea esi, [ebx+0x24]        ; Object+0x24
    push eax
    mov ecx, esi
    call ExtractMetadata       ; 0x44aff0
    
    ; Check if send callback registered
    mov eax, [ebx+0x70]        ; Get callback from offset 0x70
    test eax, eax
    je skip_callback
    
    ; Prepare callback parameters
    push 0                     ; Reserved parameter
    push edi                   ; Packet data pointer
    call eax                   ; Call OnPacketSend callback
    add esp, 8                 ; Clean stack (cdecl)
    mov esi, eax               ; Store return value
    
skip_callback:
    ; Continue with packet sending...
```

### C++ Pattern

```c
// Callback implementation
int MyOnPacketSend(void* packetData, uint32_t reserved) {
    // Log packet send
    printf("Packet processed, data=%p\n", packetData);
    
    // Can inspect packet data
    if (packetData) {
        // Process packet information
        uint8_t* data = (uint8_t*)packetData;
        // ... analyze packet ...
    }
    
    return 0;  // Continue processing
}

// Registration
void RegisterSendCallback(ConnectionObject* conn) {
    conn->callbackSend = MyOnPacketSend;  // Offset 0x70
}
```

---

## Related Functions

### ExtractMetadata (0x44aff0)

This function is called multiple times to extract packet metadata from the connection object at offset 0x24:

```c
void ExtractMetadata(void* metadataPtr, /* output params */);
```

Purpose: Extract packet header information for logging and processing

---

## Diagnostic Strings

Strings found in launcher.exe related to packet sending:

| String | Address | Context |
|--------|---------|---------|
| `"rc\libltmessaging\messageconnection.cpp"` | 0x4b7928 | Source file path |
| `"CMessageConnection::SendPacket(): Packet being sent to %d.%d.%d.%d:%d discarded by packet processing agenda.\n"` | 0x4b7970 | Send failure message |
| `"Sending headerless message %s (M: %d) to %d.%d.%d.%d:%d.\n"` | 0x4b79e0 | Send logging message |

### String Usage in Code

```assembly
; Example of logging message usage
push edi                    ; Port
push edx                    ; IP byte 4
push eax                    ; IP byte 3
push ecx                    ; IP byte 2
push edx                    ; IP byte 1
push edi                    ; Parameter
push $0x4b7970              ; Format string address
call LogFunction            ; 0x415460
```

---

## Flow/State Machine

### Packet Send Flow (Updated based on disassembly)

```
Client Request to Send
        ↓
    Extract Metadata (multiple calls to 0x44aff0)
        ↓
    Check Callback at Object+0x70
        ↓
    [If callback exists]
        ↓
    Invoke OnPacketSend(packetData, 0)
        ↓
    Store return value in ESI
        ↓
    Continue with send operation
        ↓
    Log diagnostic messages
        ↓
    Return result
```

---

## VTable Functions

Related VTable functions identified in disassembly:

| Offset | Function | Purpose |
|--------|----------|---------|
| 0x18 | SendPacket | Send network packet |
| 0x20 | VTable+0x20 | Called during send processing |
| 0x3c | VTable+0x3c | Called during send processing |

---

## Validation Status

**Status**: ✅ Validated against disassembly
**Confidence**: High
**Last Updated**: 2025-06-18
**Validated By**: Disassembly Analysis of launcher.exe

---

## Disassembly Evidence

### Key Addresses

- **SendPacket Implementation**: 0x448d00+ (function prologue at 0x448b40)
- **Callback Invocation**: 0x448edf - 0x448eeb
- **Callback Storage**: Object offset 0x70
- **Metadata Extraction Function**: 0x44aff0
- **Diagnostic String**: 0x4b7970

### Verified Details

1. ✅ Callback pointer stored at offset 0x70 in connection object
2. ✅ Callback signature: `int (*callback)(void* packetData, uint32_t reserved)`
3. ✅ Second parameter is always 0
4. ✅ Return value affects program flow
5. ✅ Conditional execution (NULL check)
6. ✅ Metadata extracted from offset 0x24 before callback

---

## Related Callbacks

- **[OnPacket](network/OnPacket.md)** - Packet received callback
- **[OnConnect](network/OnConnect.md)** - Connection established
- **[OnDisconnect](network/OnDisconnect.md)** - Connection closed

---

## TODO

- [x] Find assembly code that invokes OnPacketSend
- [x] Confirm callback storage location (offset 0x70)
- [x] Verify callback signature and parameters
- [ ] Identify callback registration mechanism
- [ ] Find where offset 0x70 is initialized/set
- [ ] Determine exact purpose of return value

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-18 | 1.0 | Initial documentation |
| 2025-06-18 | 2.0 | Updated with disassembly validation from launcher.exe |

---

**End of Document**
