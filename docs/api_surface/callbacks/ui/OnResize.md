# OnResize

## Overview

**Category**: ui
**Direction**: Launcher → Client
**Purpose**: Window resize event notification callback for UI dimension updates
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
int OnResize(ResizeEvent* event, void* userData);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `ResizeEvent*` | event | Resize event metadata and details |
| `void*` | userData | User-defined data associated with the window |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Event processed successfully |
| `int` | -1 | Processing failed, event should be ignored |

---

## Calling Convention

**Type**: `__thiscall`

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  [event parameter]
[ESP+8]  [userData parameter]

Registers (if __thiscall):
ECX = this pointer (APIObject*)
EAX = return value
```

---

## Data Structures

### ResizeEvent Structure

```c
struct ResizeEvent {
    uint32_t eventType;         // Offset 0x00: Event type (RESIZE)
    uint32_t windowId;          // Offset 0x04: Unique window identifier
    uint32_t timestamp;         // Offset 0x08: Event timestamp
    int32_t x;                  // Offset 0x0C: Window X position
    int32_t y;                  // Offset 0x10: Window Y position
    int32_t width;              // Offset 0x14: New window width
    int32_t height;             // Offset 0x18: New window height
};
```

**Size**: 16 bytes

### Related Structures

```c
// Window structure (used for window management)
struct Window {
    uint32_t id;                // Unique window identifier
    WindowState state;          // Current state (visible, hidden, etc.)
    int32_t x, y, width, height; // Position and dimensions
    void* userData;             // User-defined data pointer
};
```

---

## Constants/Enums

### WindowEventType Enumeration

| Constant | Value | Description |
|----------|-------|-------------|
| `WT_RESIZE` | 0x0002 | Window resize event (used in ResizeEvent) |

### WindowState Enumeration

| Constant | Value | Description |
|----------|-------|-------------|
| `WS_VISIBLE` | 0x0001 | Window is visible |
| `WS_HIDDEN` | 0x0002 | Window is hidden |
| `WS_DISABLED` | 0x0004 | Window is disabled |

---

## Usage

### Registration

```c
// Method 1: Using SetEventHandler
void RegisterOnResize() {
    EventHandler* handler = g_MasterDatabase->pPrimaryObject;
    int id = handler->SetEventHandler(EVENT_RESIZE, OnResize);
    
    if (id == INVALID_ID) {
        printf("Failed to register resize event handler\n");
    }
}
```

### Assembly Pattern

```assembly
; Resize event notification pattern
mov eax, [resize_event_handler]
test eax, eax
je skip_resize_event
push [event]          ; Push ResizeEvent structure
push [userData]       ; Push user data pointer
call eax              ; Call OnResize
add esp, 8            ; Clean up stack
```

### C++ Pattern

```c
// Client-side callback implementation
int MyOnResize(ResizeEvent* event, void* userData) {
    if (!event || event->eventType != WT_RESIZE) {
        return -1;
    }
    
    // Validate dimensions
    if (event->width <= 0 || event->height <= 0) {
        return -1;
    }
    
    // Handle resize event
    // Update window dimensions
    return 0;
}
```

---

## Implementation

### Launcher Side

```c
// Launcher implementation example
int LauncherOnResize(ResizeEvent* event, void* userData) {
    // Validate event
    if (!event || event->eventType == 0) {
        return -1;
    }
    
    switch (event->eventType) {
        case WT_RESIZE:
            // Update window dimensions
            break;
        default:
            return 0;
    }
    
    return 0;
}
```

### Client Side

```c
// Client callback implementation
int MyOnResize(ResizeEvent* event, void* userData) {
    if (!event || event->eventType != WT_RESIZE) {
        return -1;
    }
    
    // Validate dimensions
    if (event->width <= 0 || event->height <= 0) {
        return -1;
    }
    
    // Handle resize event - update window dimensions
    // Update UI accordingly
    
    return 0;
}

// Registration code
void RegisterOnResize() {
    EventHandler* handler = g_MasterDatabase->pPrimaryObject;
    int id = handler->SetEventHandler(EVENT_RESIZE, OnResize);
}
```

---

## Flow/State Machine

### Resize State Flow

```
[Window Visible]
    ↓ [WT_RESIZE event]
[Window Resizing]
    ↓ [Validation]
[Dimensions Validated]
    ↓ [Update UI]
[New Window Dimensions]
    ↓ [Application State Update]
[Window Rendered at New Size]
```

---

## Diagnostic Strings

Strings found in binaries related to resize events:

| String | Address | Context |
|--------|---------|---------|
| "invalid window size" | 0x004bf868 | Window validation error |

---

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `SUCCESS` | Event processed successfully |
| -1 | `ERROR_INVALID_EVENT` | Invalid event type or parameters |
| -2 | `ERROR_WINDOW_NOT_FOUND` | Window identifier not found |
| -3 | `ERROR_PERMISSION_DENIED` | Insufficient permissions for operation |
| -4 | `ERROR_INVALID_DIMENSIONS` | Window dimensions are invalid (too small, etc.) |

---

## Performance Considerations

- **Buffer Sizes**: Resize events are small (16 bytes), minimal buffering needed
- **Optimization Tips**: 
  - Use direct pointer passing to avoid copying
  - Batch resize notifications when possible
  - Validate dimensions before applying
- **Threading**: Resize events may be processed from multiple threads
- **Memory**: No additional memory allocation required for event handling

---

## Security Considerations

- **Validation**: Validate window event type and dimensions
- **Encryption**: Window dimensions should be validated against expected values
- **Authentication**: Ensure user has permission to modify windows
- **Data Sensitivity**: Window positions may reveal application layout

---

## Notes

- Resize events are sent from client to launcher for UI coordination
- The callback is event-driven, not request-driven
- Window dimensions can change dynamically during runtime
- Validation is critical to prevent invalid window states

---

## Related Callbacks

- **[OnWindowEvent](ui/OnWindowEvent.md)** ([ui/OnWindowEvent.md](ui/OnWindowEvent.md)) - General window events including resize
- **[OnFocus](ui/OnFocus.md)** ([ui/OnFocus.md](ui/OnFocus.md)) - Window focus change events
- **[OnInput](ui/OnInput.md)** ([ui/OnInput.md](ui/OnInput.md)) - Input handling for windows

---

## VTable Functions

Related VTable functions for this callback:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| N/A | N/A | N/A | Event callbacks use direct function pointers |

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 5.2
- **Address**: Resize event handler in InitClientDLL at 0x620012a0
- **Assembly**: `call dword [resize_event_handler]`
- **Evidence**: Found in client DLL initialization and window management code

---

## Documentation Status

**Status**: ⏳ Partial
**Confidence**: Medium
**Last Updated**: 2025-03-08
**Documented By**: API Surface Agent

---

## TODO

- [ ] Complete function signature from binary disassembly
- [ ] Add all diagnostic strings found in binary
- [ ] Verify resize event constants from assembly
- [ ] Document complete flow with sequence diagrams