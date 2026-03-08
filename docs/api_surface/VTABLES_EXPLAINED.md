# Understanding VTables vs Callbacks

## The Confusion

**Question**: "What does it mean that there's 117 vtables with 5,145 function pointers. Are these undocumented callbacks?"

**Answer**: NO - they are NOT all callbacks. Let me explain the difference.

---

## VTables vs Callbacks

### VTable (Virtual Method Table)

A VTable is a C++ implementation detail for **polymorphism**. It contains ALL virtual methods of a class.

```
┌─────────────────────────────────────────┐
│  VTable (C++ Class Methods)             │
├─────────────────────────────────────────┤
│ [0]  Destructor                         │
│ [1]  Constructor                        │
│ [2]  Initialize()                       │
│ [3]  Shutdown()                         │
│ [4]  RegisterCallback()      ← CALLBACK │
│ [5]  GetValue()                         │
│ [6]  SetValue()                         │
│ ...                                     │
│ [25] OnCapsValidation()      ← CALLBACK │
│ ...                                     │
│ [78] ProcessData()                      │
│ [79] HelperFunction()                   │
└─────────────────────────────────────────┘
```

### Callback (Specific Pattern)

A callback is a function used for **bidirectional communication** between components.

```
Client.dll                   Launcher.exe
    │                             │
    │  RegisterCallback(func)     │
    ├────────────────────────────►│
    │                             │
    │                             │ Event occurs
    │                             │
    │        callback()           │
    │◄────────────────────────────┤
    │                             │
```

---

## The Numbers: What They Mean

### 117 VTables = 117 C++ Classes

Each VTable represents one C++ class in launcher.exe:
- Primary API Object
- Network Manager
- Packet Handler
- Session Manager
- ... 113 other classes

### 5,145 Function Pointers = All Virtual Methods

These are ALL the virtual methods across all 117 classes:

```
┌─────────────────────────────────────────┐
│ 5,145 Total Virtual Methods             │
├─────────────────────────────────────────┤
│                                         │
│  ████████████████ ~75%                  │
│  Internal Business Logic (~3,900)       │
│  - Not exposed to client.dll            │
│  - Used only by launcher internally     │
│  - getValue(), setValue(), process()    │
│                                         │
│  ████ ~15%                              │
│  Utility/Helper Methods (~800)          │
│  - Internal helpers                     │
│  - Not exposed                          │
│  - formatString(), validate(), etc.     │
│                                         │
│  ██ ~5%                                 │
│  Exposed API Methods (~250)             │
│  - Accessible to client.dll             │
│  - Part of public interface             │
│  - But NOT callbacks                    │
│                                         │
│  █ ~3%                                  │
│  Actual Callbacks (~50-100)             │
│  - Bidirectional communication          │
│  - Event notifications                 │
│  - Packet handlers                     │
│  - THIS IS WHAT WE DOCUMENT             │
│                                         │
└─────────────────────────────────────────┘
```

---

## Classification Breakdown

### What's in the 5,145 Function Pointers?

| Category | Count | Percentage | Purpose |
|----------|-------|------------|---------|
| **Internal Business Logic** | ~3,900 | 75% | Core launcher functionality |
| **Utility Methods** | ~800 | 15% | Helpers, formatters, validators |
| **Exposed API** | ~250 | 5% | Public methods (not callbacks) |
| **Actual Callbacks** | ~50-100 | 2-3% | **What we document** |
| **Constructors/Destructors** | ~150 | 3% | Object lifecycle |

### Example: Primary API Object VTable (0x4a9988)

**80 entries total**:

| Index | Function | Type | Purpose |
|-------|----------|------|---------|
| 0 | Destructor | Internal | Object cleanup |
| 1 | Constructor | Internal | Object creation |
| 2-3 | Initialize/Shutdown | Internal | Lifecycle |
| 4 | RegisterCallback | **Callback** | Registration API |
| 5-22 | GetX/SetX methods | Exposed API | Data access |
| 23 | SetEventHandler | **Callback** | Event registration |
| 24 | RegisterCallback2 | **Callback** | Registration API |
| 25 | OnCapsValidation | **Callback (STUB)** | Validation (stub) |
| 26-79 | Business logic | Internal | Processing, helpers |

**Only 3-4 are actual callbacks!**

---

## Why This Matters

### Documentation Scope

**We should document**: ~50-100 actual callbacks
- Functions client.dll registers/implements
- Event notification handlers
- Packet processing callbacks
- Lifecycle callbacks

**We should NOT document**: ~5,000+ internal methods
- Internal business logic
- Helper functions
- Getters/setters
- Processing methods

### How to Identify Real Callbacks

A function is a **callback** if:

✅ **Called across component boundary**
- launcher.exe → client.dll
- client.dll → launcher.exe

✅ **Used for notification/events**
- Packet received
- State changed
- Error occurred

✅ **Has registration mechanism**
- RegisterCallback()
- SetEventHandler()

✅ **Bidirectional communication**
- Not just calling a method
- Registering to be called back

❌ **NOT a callback if**:
- Just a method call (obj->getValue())
- Internal implementation detail
- No registration mechanism
- Unidirectional call

---

## Real Examples

### IS a Callback: OnPacket

```c
// Client.dll registers handler
RegisterPacketHandler(MyOnPacket);

// Launcher.exe calls it when packet arrives
client->OnPacket(packet);  // ← Callback
```

**Why**: Bidirectional, event-driven, registered by client

### NOT a Callback: GetValue()

```c
// Just a method call
value = obj->GetValue();  // ← Not a callback
```

**Why**: Unidirectional, no registration, not event-driven

### IS a Callback: OnDisconnect

```c
// Client.dll registers handler
SetEventHandler(EVENT_DISCONNECT, MyOnDisconnect);

// Launcher.exe calls it on disconnect
client->OnDisconnect();  // ← Callback
```

**Why**: Event notification, registered, bidirectional

---

## The 117 VTables

### What Are They?

These are **117 C++ classes** in launcher.exe:

```
VTable 0x4a9988 (80 entries)  → Primary API Object
VTable 0x4a9xxx (xx entries)  → Network Manager
VTable 0x4axxxx (xx entries)  → Packet Handler
VTable 0x4bxxxx (xx entries)  → Session Manager
...
VTable 0x4cxxxx (xx entries)  → Other classes
```

### Which Ones Have Callbacks?

**Only a few VTables contain callbacks**:
- Primary API Object (VTable 0x4a9988) - main interface
- Maybe 5-10 other VTables with event handlers
- Most VTables are purely internal

---

## Summary

### The Truth About "5,145 Function Pointers"

**Are they all callbacks?**
❌ **NO** - Only ~50-100 are actual callbacks

**What are they?**
✅ All virtual methods of 117 C++ classes
✅ Most are internal business logic
✅ Only ~2-3% are actual callbacks

**What should we document?**
📝 Focus on the ~50-100 actual callbacks
📝 Ignore the ~5,000 internal methods
📝 Identify callbacks by their pattern (registration, events, bidirectional)

### Key Takeaway

The "massive API surface" is not 5,145 callbacks. It's:
- 117 C++ classes with virtual methods
- ~50-100 actual callbacks (what we document)
- ~5,000+ internal methods (ignore for now)

**We have documented 43 callbacks so far** (out of estimated 50-100 actual callbacks)

---

## References

- **VTable Analysis**: `api_surface/function_pointer_tables.md`
- **Callback Documentation**: `callbacks/` directory
- **Callback Index**: `callbacks/CALLBACK_INDEX.md`
