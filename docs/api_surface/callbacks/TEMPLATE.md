# [CallbackName]

## Overview

**Category**: [Category] (lifecycle/network/game/ui/monitor/registration)
**Direction**: [Launcher → Client] OR [Client → Launcher]
**Purpose**: [Brief description of what this callback does]
**VTable Index**: [Index number] (if applicable)
**Byte Offset**: [Hex offset] (if applicable)

---

## Function Signature

```c
[ReturnType] [CallbackName]([ParameterType1] [param1], [ParameterType2] [param2], ...);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `[Type]` | `[name]` | [Description] |
| `[Type]` | `[name]` | [Description] |
| `[Type]` | `[name]` | [Description] |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `[Type]` | `[value]` | [Description] |
| `[Type]` | `[value]` | [Description] |

---

## Calling Convention

**Type**: `__thiscall` OR `__stdcall` OR `__cdecl`

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  [parameter1 description]
[ESP+8]  [parameter2 description]
...

Registers (if __thiscall):
ECX = this pointer (APIObject*)
EAX = return value
```

---

## Data Structures

### [StructureName] Structure

```c
struct [StructureName] {
    [Type] [fieldName];          // Offset 0x00: [Description]
    [Type] [fieldName];          // Offset 0x04: [Description]
    [Type] [fieldName];          // Offset 0x08: [Description]
    // ... additional fields
};
```

**Size**: [X] bytes

### Related Structures

```c
// Add any related structures here
struct [RelatedStructure] {
    // fields
};
```

---

## Constants/Enums

### [EnumName] Enumeration

| Constant | Value | Description |
|----------|-------|-------------|
| `[CONSTANT_NAME]` | `0x0001` | [Description] |
| `[CONSTANT_NAME]` | `0x0002` | [Description] |
| `[CONSTANT_NAME]` | `0x0003` | [Description] |

### [FlagName] Flags

| Flag | Bit | Description |
|------|-----|-------------|
| `[FLAG_NAME]` | 0 | [Description] |
| `[FLAG_NAME]` | 1 | [Description] |
| `[FLAG_NAME]` | 2 | [Description] |

---

## Usage

### Registration

How to register this callback:

```c
// Method 1: Using RegisterCallback
[Registration code example]

// Method 2: Using SetEventHandler
[Registration code example]

// Method 3: Using RegisterCallback2
[Registration code example]
```

### Assembly Pattern

```assembly
; [Description of assembly pattern]
mov [register], [value]         ; [Comment]
mov [register], [value]         ; [Comment]
push [parameter]                ; [Comment]
call [function]                 ; [Comment]
add esp, [cleanup]              ; [Comment]
```

### C++ Pattern

```c
// [Description of C++ usage]
[ObjectName]* obj = [Get object];
[ReturnType] result = obj->[MethodName]([parameters]);
```

---

## Implementation

### Launcher Side

```c
// Launcher implementation example
[ReturnType] [ModuleName]_[CallbackName]([Parameters]) {
    // Implementation
    [Code]

    return [value];
}
```

### Client Side

```c
// Client callback implementation
[ReturnType] My[CallbackName]([Parameters]) {
    // Handle callback
    [Code]

    return [value];
}

// Registration code
void Register[CallbackName]() {
    [Registration code]
}
```

---

## Flow/State Machine

### [Flow/State Name]

```
[State1]
    ↓ [Event/Action]
[State2]
    ↓ [Event/Action]
[State3]
    ↓ [Event/Action]
[State4]
```

### Sequence Diagram

```
[Participant1]          [Participant2]
     |                       |
     |--[Action1]---------->|
     |                       |-- [Process]
     |<--[Response]---------|
     |                       |
```

---

## Diagnostic Strings

Strings found in binaries related to this callback:

| String | Address | Context |
|--------|---------|---------|
| `"[String text]"` | `0x[Address]` | [Purpose/Context] |
| `"[String text]"` | `0x[Address]` | [Purpose/Context] |

---

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| `0` | `SUCCESS` | Operation successful |
| `-1` | `ERROR_INVALID_PARAM` | Invalid parameter |
| `-2` | `ERROR_[NAME]` | [Description] |

---

## Performance Considerations

- **Buffer Sizes**: [Recommendations]
- **Optimization Tips**: [Tips]
- **Threading**: [Thread safety considerations]
- **Memory**: [Memory management notes]

---

## Security Considerations

- **Validation**: [What to validate]
- **Encryption**: [Encryption requirements]
- **Authentication**: [Auth requirements]
- **Data Sensitivity**: [Sensitive data handling]

---

## Notes

- **[Note 1]**: [Description]
- **[Note 2]**: [Description]
- **[Note 3]**: [Description]
- **Common Pitfalls**: [What to avoid]
- **Best Practices**: [Recommendations]

---

## Related Callbacks

- **[RelatedCallback1]** ([category/CallbackName.md](category/CallbackName.md)) - [Brief description]
- **[RelatedCallback2]** ([category/CallbackName.md](category/CallbackName.md)) - [Brief description]
- **[RelatedCallback3]** ([category/CallbackName.md](category/CallbackName.md)) - [Brief description]

---

## VTable Functions

Related VTable functions for this callback:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| `[index]` | `0x[offset]` | `[FunctionName]` | [Purpose] |
| `[index]` | `0x[offset]` | `[FunctionName]` | [Purpose] |

---

## References

- **Source**: `[filename].md` Section [X.X]
- **Address**: [Memory address or VTable index]
- **Assembly**: `[Assembly instruction or pattern]`
- **Evidence**: [What confirms this callback exists]
- **Related Analysis**: [Links to other documentation]

---

## Documentation Status

**Status**: ✅ Complete OR ⏳ Partial OR ❌ TODO
**Confidence**: High/Medium/Low
**Last Updated**: YYYY-MM-DD
**Documented By**: [Name/Handle]

---

## TODO

- [ ] [Item needing investigation]
- [ ] [Item needing investigation]
- [ ] [Item needing investigation]

---

## Example Usage

### Complete Working Example

```c
// Full working example of using this callback
#include "[headers]"

// Callback implementation
[ReturnType] My[CallbackName]([Parameters]) {
    printf("[Callback] called with [info]\n");

    // Process callback
    [Implementation]

    return [value];
}

// Registration
int main() {
    // Initialize
    [Setup code]

    // Register callback
    [Registration code]

    // Main loop
    [Main loop code]

    return 0;
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| YYYY-MM-DD | 1.0 | Initial documentation |
| YYYY-MM-DD | 1.1 | [Description of changes] |

---

**End of Template**
