# How to Use the Callback Documentation Template

## Overview

This guide explains how to use `TEMPLATE.md` to document callbacks consistently.

---

## Quick Start

1. **Target file** callbacks/[category]/[CallbackName].md

2. **Fill in required sections** (marked with [brackets])

3. **Delete optional sections** that don't apply

4. **Update the index**: `callbacks/CALLBACK_INDEX.md`

---

## Template Sections

### ✅ Required Sections (Always Include)

These sections should be in **every** callback documentation:

1. **Overview** - Basic info (category, direction, purpose)
2. **Function Signature** - Exact C function signature
3. **Parameters** - Table of all parameters
4. **Return Value** - What the callback returns
5. **Usage** - How to register and use the callback
6. **Notes** - Important information
7. **Related Callbacks** - Links to related callbacks
8. **References** - Source documentation
9. **Documentation Status** - Completion status

### ⚙️ Conditional Sections (Include When Applicable)

Include these sections **only if relevant**:

- **Calling Convention** - For VTable functions
- **Data Structures** - If callback uses structures
- **Constants/Enums** - If callback uses constants
- **Flow/State Machine** - For state-based callbacks
- **Diagnostic Strings** - If strings found in binary
- **Error Codes** - If callback can fail
- **Performance Considerations** - For high-frequency callbacks
- **Security Considerations** - For sensitive operations
- **VTable Functions** - If related to VTable
- **Implementation** - If providing code examples

### ❌ Sections to Remove

**Delete these placeholder sections** if not applicable:

- Empty structure definitions
- Missing diagnostic strings
- Unused constants/enums
- Irrelated VTable functions

---

## Filling Out the Template

### 1. Overview Section

```markdown
# OnPacket

## Overview

**Category**: network
**Direction**: Launcher → Client
**Purpose**: Network packet received/sent notification callback
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A
```

**Guidelines**:
- Category: Choose from lifecycle, network, game, ui, monitor, registration
- Direction: "Launcher → Client" for events, "Client → Launcher" for API calls
- Purpose: One-line description
- VTable Index: Only for VTable functions (0-30)
- Byte Offset: Hex offset (e.g., 0x10) for VTable functions

### 2. Function Signature

```markdown
## Function Signature

```c
int OnPacket(PacketEvent* packetEvent, void* packetData, uint32_t packetSize);
```
```

**Guidelines**:
- Use standard C syntax
- Include all parameters
- Match actual binary signature
- Use meaningful parameter names

### 3. Parameters

```markdown
### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `PacketEvent*` | packetEvent | Packet event metadata |
| `void*` | packetData | Raw packet data buffer |
| `uint32_t` | packetSize | Size of packet data |
```

**Guidelines**:
- Document every parameter
- Be specific about purposes
- Include units where applicable (bytes, milliseconds, etc.)

### 4. Return Value

```markdown
### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Packet processed successfully |
| `int` | -1 | Processing failed |
| `int` | 1 | Packet consumed (don't process further) |
```

**Guidelines**:
- List all possible return values
- Explain meaning of each value
- Include error codes if applicable

### 5. Data Structures

```markdown
### PacketEvent Structure

```c
struct PacketEvent {
    uint32_t eventType;         // Event type
    uint32_t connId;            // Connection ID
    uint16_t messageType;       // Message type
    uint16_t flags;             // Packet flags
};
```

**Size**: 12 bytes
```

**Guidelines**:
- Include byte offsets in comments
- Specify total size
- Document all fields
- Use standard C types (uint32_t, uint16_t, etc.)

### 6. Usage

```markdown
### Registration

```c
CallbackRegistration reg;
reg.eventType = EVENT_PACKET;
reg.callbackFunc = MyOnPacket;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
mov eax, [packet_callback]
test eax, eax
je skip_callback
push packetSize
push packetData
push packetEvent
call eax
add esp, 12
```
```

**Guidelines**:
- Show actual registration code
- Include assembly if available from disassembly
- Provide C++ pattern for modern usage

### 7. Implementation

```markdown
### Launcher Side

```c
int SendPacket(ConnectionObject* conn, void* data, uint32_t size) {
    // Validate
    if (!conn || !data) return -1;

    // Send packet
    // ...

    return 0;
}
```

### Client Side

```c
int MyOnPacket(PacketEvent* event, void* data, uint32_t size) {
    printf("Packet received: %d bytes\n", size);
    return 0;
}
```
```

**Guidelines**:
- Provide realistic implementation examples
- Show both launcher and client sides
- Include error handling
- Keep examples concise but complete

### 8. Diagnostic Strings

```markdown
## Diagnostic Strings

| String | Address | Context |
|--------|---------|---------|
| "Delete callback - ID %d\n" | 0x6289f21c | Callback cleanup |
| "Could not load exception callback" | 0x628f17d4 | Error handling |
```

**Guidelines**:
- Only include strings actually found in binary
- Provide exact addresses
- Explain context/purpose

### 9. References

```markdown
## References

- **Source**: `client_dll_callback_analysis.md` Section 4.2
- **Address**: VTable index 4, byte offset 0x10
- **Assembly**: `call dword [edx + 0x10]`
- **Evidence**: Found in InitClientDLL at 0x620012a0
```

**Guidelines**:
- Link to source documentation
- Include addresses and offsets
- Note evidence that confirms callback exists

---

## Documentation Status Levels

Use these status indicators:

| Status | Symbol | Meaning |
|--------|--------|---------|
| Complete | ✅ | Fully documented with all details |
| Partial | ⏳ | Documented but missing some details |
| TODO | ❌ | Not yet documented |

### Confidence Levels

| Level | When to Use |
|-------|-------------|
| **High** | Verified in disassembly, has string evidence, complete structures |
| **Medium** | Inferred from context, partial evidence, reasonable assumptions |
| **Low** | Speculative, needs verification, incomplete information |

---

## Example: Fully Documented Callback

See these examples:

- ✅ **[RegisterCallback](registration/RegisterCallback.md)** - Registration function
- ✅ **[OnConnect](network/OnConnect.md)** - Network event callback
- ✅ **[OnPacket](network/OnPacket.md)** - Complex callback with structures

---

## Checklist Before Publishing

Before marking a callback as "Complete", verify:

- [ ] All required sections filled
- [ ] Function signature matches binary
- [ ] All parameters documented
- [ ] Return values explained
- [ ] Code examples tested/verified
- [ ] Related callbacks linked
- [ ] References to source documentation
- [ ] No placeholder text remaining
- [ ] File added to CALLBACK_INDEX.md
- [ ] Consistent formatting with other callbacks

---

## Common Mistakes to Avoid

❌ **Don't**:
- Leave placeholder text ([Description])
- Invent parameters not in binary
- Copy-paste without adapting
- Forget to update index
- Leave TODO sections without marking status

✅ **Do**:
- Use actual addresses from binary
- Reference real source documentation
- Provide working code examples
- Link to related callbacks
- Mark confidence level accurately

---

## Getting Help

If you need help documenting a callback:

1. Check existing documented callbacks for examples
2. Search the analysis documents for relevant information
3. Look for diagnostic strings in the binary
4. Check VTable offsets in function_pointer_tables.md
5. Review client_dll_callback_analysis.md for patterns

---

## Template Maintenance

Update this template when:

- New callback patterns discovered
- New VTable offsets identified
- Better documentation practices developed
- New sections become standard

---

**Last Updated**: 2025-06-17
**Version**: 1.0
