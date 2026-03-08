# Callback Registration Analysis - Phase 2.3

## Executive Summary

This document analyzes the callback registration mechanisms in launcher.exe, focusing on how the internal API surface (discovered in Phase 2.2) is initialized and used through virtual function table (vtable) assignments.

## Key Findings

### 1. Callback Registration Pattern

The primary callback registration mechanism uses **vtable assignment** pattern:

```assembly
mov dword [ecx], 0x4a9988    ; Assign vtable to object
mov dword [esi], 0x4a9b40    ; Alternative register usage
```

This is the standard C++ object construction pattern where:
- First field of object = pointer to vtable
- Vtable contains array of function pointers
- Object methods are called via vtable indirection

### 2. Discovered Registration Functions

#### Function 1: Object Constructor at 0x00401000
```assembly
0x00401000  mov eax, 0x4a9908
0x00401020  mov dword [ecx], 0x4a9988    ; Register vtable 1
0x00401080  mov dword [esi], 0x4a9988    ; Register vtable 1 again
```
**Purpose**: Initializes object with vtable at 0x4a9988 (80 entries)
**Vtable**: Contains functions like 0x401000, 0x4012a0, 0x441790, etc.

#### Function 2: Object Constructor at 0x004012d0
```assembly
0x004012d0  mov eax, 0x4a9b0c
0x004012e0  mov dword [ecx], 0x4a9b40    ; Register vtable 2
```
**Purpose**: Initializes object with vtable at 0x4a9b40 (88 entries)
**Vtable**: Different object type with 88 function pointers

#### Function 3: Object Constructor at 0x004017b0
```assembly
0x004017b0  mov dword [ecx], 0x4a9df8    ; Register vtable 3
```
**Purpose**: Initializes object with vtable at 0x4a9df8 (91 entries)

#### Function 4: Object Constructor at 0x00401d10
```assembly
0x00401d10  mov eax, 0x4a9f98
0x00401d3a  mov dword [esi], 0x4a9fd8    ; Register vtable 4
```
**Purpose**: Initializes object with vtable at 0x4a9fd8 (80 entries)

### 3. Vtable Registration Map

| Address Range | Vtable Address | Entries | Registration Function | Object Type |
|---------------|----------------|---------|----------------------|-------------|
| 0x4a9988-0x4a9ac8 | 0x4a9988 | 80 | 0x00401000 | Type A |
| 0x4a9b40-0x4a9c9c | 0x4a9b40 | 88 | 0x004012d0 | Type B |
| 0x4a9df8-0x4a9f60 | 0x4a9df8 | 91 | 0x004017b0 | Type C |
| 0x4a9fd8-0x4aa114 | 0x4a9fd8 | 80 | 0x00401d10 | Type D |
| 0x4aabb0-0x4aac58 | 0x4aabb0 | 43 | Multiple | Type E |
| 0x4ab528-0x4ab664 | 0x4ab528 | 80 | 0x004078b0 | Type F |
| 0x4abd60-0x4abec0 | 0x4abd60 | 89 | 0x004095a0 | Type G |
| 0x4ac8f0-0x4aca58 | 0x4ac8f0 | 91 | 0x0040b8d0 | Type H |
| 0x4aca90-0x4acbcc | 0x4aca90 | 80 | 0x0040c890 | Type I |
| 0x4acd98-0x4aced8 | 0x4acd98 | 81 | 0x0040cc00 | Type J |

### 4. Callback Registration Mechanism

The callback registration follows a **three-tier architecture**:

#### Tier 1: Static Vtable Definition (Compile-time)
- 117 vtables stored in `.rdata` section (0x4a9000-0x4c6000)
- Each vtable is an array of function pointers
- Total of 5,145 function pointers discovered

#### Tier 2: Object Construction (Runtime)
- Constructor functions assign vtable to object's first field
- Pattern: `mov dword [object_ptr], vtable_address`
- Multiple objects can share the same vtable (inheritance)

#### Tier 3: Virtual Method Invocation (Runtime)
- Code calls methods via vtable: `call dword [object + vtable_offset]`
- Example: `call dword [ecx + 0x58]` calls method at offset 0x58 in vtable

### 5. Common Vtable Patterns

#### Pattern A: MFC-style Vtable (Most Common)
```
Offset 0x00: Destructor
Offset 0x04: Constructor wrapper
Offset 0x08: RTTI pointer (0x441790)
Offset 0x0C: Helper function (0x401030)
Offset 0x10-0x50: Virtual methods (30-40 entries)
```

#### Pattern B: Event Handler Vtable
```
Offset 0x00-0x10: Event handlers
Offset 0x14-0x28: Notification callbacks
Offset 0x2C-0x40: User interaction handlers
```

### 6. Registration Function Signatures

Based on disassembly analysis, registration functions have these characteristics:

```c
// Constructor signature (typical)
void* __thiscall ObjectType_Constructor(void* this) {
    this->vtable = &ObjectType_vtable;  // mov dword [ecx], vtable_addr
    // ... initialize other fields ...
    return this;
}
```

### 7. Callback Registration Locations

Found **15+ distinct registration locations** in first 3000 instructions:

| Address | Instruction | Vtable | Purpose |
|---------|-------------|--------|---------|
| 0x401020 | mov [ecx], 0x4a9988 | Type A | Main window object |
| 0x401080 | mov [esi], 0x4a9988 | Type A | Alternative registration |
| 0x4012e0 | mov [ecx], 0x4a9b40 | Type B | Dialog object |
| 0x401550 | mov [esi], 0x4a9b40 | Type B | Alternative dialog |
| 0x4017b0 | mov [ecx], 0x4a9df8 | Type C | Control object |
| 0x4017fa | mov [esi], 0x4a9e20 | Type C variant | Subclassed control |
| 0x401d3a | mov [esi], 0x4a9fd8 | Type D | Custom control |
| 0x401d9f | mov [ecx], 0x4a9df8 | Type C | Another control |
| 0x401db1 | mov [ecx], 0x4a9df8 | Type C | Yet another control |

### 8. Notification Function Mapping

Based on vtable structure analysis, notification functions appear at specific offsets:

| Vtable Offset | Purpose | Example Function |
|---------------|---------|------------------|
| 0x00 | Destructor | 0x00401000 |
| 0x04 | Second constructor | 0x004012a0 |
| 0x08 | RTTI/typeinfo | 0x00441790 |
| 0x0C | Helper/dummy | 0x00401030 |
| 0x10-0x2C | Event handlers | Various (0x48b7f6, etc.) |
| 0x30-0x4C | Message handlers | Various (0x48b784, etc.) |
| 0x50-0x68 | Command handlers | Various (0x48b6e0, etc.) |
| 0x6C-0x80 | Data methods | Various (0x48b680, etc.) |

### 9. Callback Flow Analysis

```
User Action
    ↓
Windows Message Queue
    ↓
MFC Message Pump
    ↓
Window Proc
    ↓
Object->vtable[handler_offset](object, params)
    ↓
Handler Function (e.g., 0x4012a0)
    ↓
Business Logic
    ↓
Return to MFC framework
```

### 10. Client.dll Integration Points

Based on the API surface discovered, client.dll likely:
1. **Receives vtable pointers** from launcher during initialization
2. **Calls virtual methods** through vtable indirection
3. **Registers its own callbacks** via function pointer tables

**Critical Integration Address**: `SetMasterDatabase` export at 0x004143f0
- This is the ONLY exported function
- Likely passes master API table to client.dll
- Client.dll uses this to discover all internal APIs

## Technical Details

### Vtable Entry Structure

Each vtable entry is 4 bytes (32-bit pointer):
```c
struct vtable_entry {
    void (*function_ptr)(void);  // 4-byte function address
};
```

### Object Structure (C++ This Pointer)

```c
struct MFC_Object {
    void** vtable;          // +0x00: Pointer to vtable
    void* data_member_1;    // +0x04: First data member
    void* data_member_2;    // +0x08: Second data member
    // ... more members ...
};
```

### Virtual Method Call Pattern

```assembly
; Method call via vtable
mov ecx, object_ptr           ; Set 'this' pointer
mov eax, [ecx]                ; Get vtable pointer
call dword [eax + offset*4]   ; Call virtual method
```

## Conclusions

1. **Callback registration is static**: Vtables are defined at compile-time in `.rdata` section
2. **Registration is constructor-based**: Object constructors assign vtables during initialization
3. **Large API surface confirmed**: 117 vtables with 5,145 function pointers
4. **MFC architecture**: Standard MFC/C++ vtable-based polymorphism
5. **Client.dll integration**: Likely receives vtable pointers via SetMasterDatabase export

## Next Steps

1. **Phase 2.4**: Extract data structures from `.data` section
2. **Phase 3.1**: Analyze how client.dll discovers launcher functions
3. **Phase 3.2**: Map runtime callbacks between launcher and client.dll
4. **Phase 4.1**: Reverse engineer function signatures for vtable methods
5. **Phase 4.3**: Document callback mechanisms in detail

## Files Generated

- `function_pointer_tables.md` - Complete vtable enumeration
- `callback_registration_analysis.md` - This document
- `entry_point_analysis.md` - Entry point and initialization analysis

## References

- MFC Source Code (Microsoft Foundation Classes)
- PE Format Specification
- Radare2 Analysis Tools
- Function pointer tables at addresses 0x4a9000-0x4c6000
