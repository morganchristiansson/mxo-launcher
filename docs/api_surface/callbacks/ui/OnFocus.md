# OnFocus

## Overview

**Category**: UI  
**Direction**: Launcher → Client  
**Purpose**: Window focus change notification callback

---

## Function Signature

```c
void OnFocus(FocusEvent* focusEvent, uint32_t windowId, uint32_t eventType);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `FocusEvent*` | focusEvent | Focus event details and state |
| `uint32_t` | windowId | Window identifier |
| `uint32_t` | eventType | Focus event type (FOCUS_IN, FOCUS_OUT) |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value |

---

## Focus Event Types

The focus system handles various event types:

| Event Type | Description |
|------------|-------------|
| `FOCUS_IN` | Window gained focus (became active window) |
| `FOCUS_OUT` | Window lost focus (other window became active) |

---

## Focus Data Structures

### FocusEvent Structure

```c
struct FocusEvent {
    uint32_t eventType;        // Event type (FOCUS_IN, FOCUS_OUT)
    uint32_t windowId;         // Target window identifier
    uint32_t previousWindowId; // Previous active window (if any)
    uint32_t timestamp;        // Event timestamp
};
```

### WindowFocus Structure

```c
struct WindowFocus {
    void* pVTable;              // 0x00: Virtual function table
    uint32_t focusState;        // 0x04: Current focus state
    
    // Focus tracking
    uint32_t activeWindowId;    // 0x08: Active window identifier
    uint32_t focusedWindowId;   // 0x0C: Focused window identifier
    
    // Callback registration
    void* focusCallback;        // 0x10: Registered focus callback
    void* callbackUserData;     // 0x14: User data for callback
};
```

---

## Usage

### Registration

Register focus callback during initialization:

```c
// Register focus callback
CallbackRegistration reg;
reg.eventType = EVENT_FOCUS;
reg.callbackFunc = OnFocus;
reg.userData = NULL;
reg.priority = 50;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Launcher invokes OnFocus callback
mov eax, [focus_callback]     ; Load callback pointer
test eax, eax                 ; Check if NULL
je skip_callback              ; Skip if no callback

; Prepare parameters
push eventType               ; Focus event type
push windowId                ; Window identifier
push focusEvent              ; Focus event structure
call eax                      ; Invoke callback
add esp, 12                   ; Cleanup stack

skip_callback:
```

---

## Implementation

### Launcher Side

```c
// Trigger focus event
void OnFocusReceived(FocusEvent* input) {
    // Find registered callback
    CallbackEntry* entry = FindCallbackByType(EVENT_FOCUS);
    if (entry && entry->callback) {
        OnFocusCallback callback = (OnFocusCallback)entry->callback;
        uint32_t windowId = input->windowId;
        uint32_t eventType = input->eventType;
        
        callback(input, windowId, eventType);
    }
}

// Handle window focus gain
void HandleWindowFocusGain(uint32_t windowId) {
    FocusEvent event;
    event.eventType = FOCUS_IN;
    event.windowId = windowId;
    event.previousWindowId = GetActiveWindowId();
    event.timestamp = GetTickCount();
    
    // Mark window as focused
    SetWindowFocus(windowId, true);
    
    // Trigger callback
    OnFocusReceived(&event);
}

// Handle window focus loss
void HandleWindowFocusLoss(uint32_t windowId) {
    FocusEvent event;
    event.eventType = FOCUS_OUT;
    event.windowId = windowId;
    event.previousWindowId = GetActiveWindowId();
    event.timestamp = GetTickCount();
    
    // Clear focus state
    SetWindowFocus(windowId, false);
    
    // Trigger callback
    OnFocusReceived(&event);
}

// Update active window
void SetActiveWindow(uint32_t windowId) {
    FocusEvent event;
    event.eventType = FOCUS_IN;
    event.windowId = windowId;
    event.previousWindowId = GetActiveWindowId();
    event.timestamp = GetTickCount();
    
    // Update internal state
    g_ActiveWindowId = windowId;
    
    // Trigger callback
    OnFocusReceived(&event);
}
```

### Client Side

```c
// Focus callback implementation
void MyOnFocusCallback(FocusEvent* focusEvent, uint32_t windowId, uint32_t eventType) {
    switch (focusEvent->eventType) {
        case FOCUS_IN:
            // Window gained focus
            HandleWindowFocus(windowId);
            break;
            
        case FOCUS_OUT:
            // Window lost focus
            HandleWindowFocusLoss(windowId);
            break;
    }
}

// Focus gain handler
void HandleWindowFocus(uint32_t windowId) {
    // Make window active and visible
    ShowWindow(windowId, true);
    
    // Bring window to front
    BringWindowToFront(windowId);
    
    // Update UI state
    UpdateActiveWindow(windowId);
    
    // Trigger keyboard input if needed
    TriggerKeyboardInput(windowId);
}

// Focus loss handler
void HandleWindowFocusLoss(uint32_t windowId) {
    // Hide window if minimized
    if (IsWindowMinimized(windowId)) {
        ShowWindow(windowId, false);
    }
    
    // Update UI state
    UpdateActiveWindow(GetActiveWindowId());
    
    // Disable keyboard input
    DisableKeyboardInput(windowId);
}

// Register focus callback
void RegisterFocusCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_FOCUS;
    reg.callbackFunc = MyOnFocusCallback;
    reg.userData = NULL;
    reg.priority = 50;
    reg.flags = 0;
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*regFunc)(APIObject*, CallbackRegistration*);
    regFunc = (int (*)(APIObject*, CallbackRegistration*))vtable->functions[23];
    
    int callbackId = regFunc(obj, &reg);
    printf("Registered focus callback, ID=%d\n", callbackId);
}

// Unregister focus callback
void UnregisterFocusCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_FOCUS;
    reg.callbackFunc = NULL;
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    VTable* vtable = obj->pVTable;
    
    int (*unregFunc)(APIObject*, uint32_t);
    unregFunc = (int (*)(APIObject*, uint32_t))vtable->functions[5];
    
    int result = unregFunc(obj, 0);  // Use callback ID from registration
    printf("Unregistered focus callback, result=%d\n", result);
}

// Tab key handling
void HandleTabKey(uint32_t windowId) {
    FocusEvent event;
    uint32_t activeWindow = GetActiveWindowId();
    
    // Cycle through windows
    if (activeWindow != windowId) {
        event.eventType = FOCUS_IN;
        event.windowId = activeWindow;
        event.previousWindowId = windowId;
        event.timestamp = GetTickCount();
        
        SetActiveWindow(activeWindow);
    }
}
```

---

## Focus State Management

### Active Window Tracking

```c
// Track which window is currently active
uint32_t g_ActiveWindowId = 0;  // Initially no active window

void SetActiveWindow(uint32_t windowId) {
    if (windowId != g_ActiveWindowId) {
        FocusEvent event;
        event.eventType = FOCUS_IN;
        event.windowId = windowId;
        event.previousWindowId = g_ActiveWindowId;
        event.timestamp = GetTickCount();
        
        // Update internal state
        g_ActiveWindowId = windowId;
        
        // Trigger callback
        OnFocusReceived(&event);
    }
}

uint32_t GetActiveWindowId(void) {
    return g_ActiveWindowId;
}
```

### Window Focus State

```c
// Track focus state for each window
typedef struct {
    uint32_t windowId;         // Window identifier
    bool isFocused;           // Currently focused
    bool wasFocused;          // Previously focused
    bool canFocus;            // Can receive focus
} WindowFocusState;

WindowFocusState g_WindowFocusStates[MAX_WINDOWS];

void SetWindowFocus(uint32_t windowId, bool focused) {
    if (windowId < MAX_WINDOWS) {
        if (focused) {
            g_WindowFocusStates[windowId].isFocused = true;
            g_WindowFocusStates[windowId].wasFocused = false;
        } else {
            g_WindowFocusStates[windowId].isFocused = false;
            g_WindowFocusStates[windowId].wasFocused = true;
        }
    }
}

bool IsWindowFocused(uint32_t windowId) {
    return windowId < MAX_WINDOWS && g_WindowFocusStates[windowId].isFocused;
}
```

---

## Focus Event Flow

### Typical Focus Sequence

```
1. User presses Tab key or clicks another window
   └─> Windows sends message to launcher

2. Launcher processes input
   └─> Updates internal focus state
   └─> Creates FocusEvent structure

3. Launcher triggers callback
   └─> Calls registered OnFocus function
   └─> Passes event data

4. Client processes event
   └─> Makes window active/focused
   └─> Handles specific event type

5. Event cleanup
   └─> Update internal state
   └─> Handle previous window
```

---

## Usage Examples

### Tab Key Cycling

```c
void HandleTabInput(uint32_t windowId) {
    FocusEvent event;
    uint32_t activeWindow = GetActiveWindowId();
    
    // Cycle through windows (forward)
    if (activeWindow != windowId) {
        event.eventType = FOCUS_IN;
        event.windowId = activeWindow;
        event.previousWindowId = windowId;
        event.timestamp = GetTickCount();
        
        SetActiveWindow(activeWindow);
    }
}

void HandleShiftTabInput(uint32_t windowId) {
    FocusEvent event;
    uint32_t activeWindow = GetActiveWindowId();
    
    // Cycle through windows (backward)
    if (activeWindow != windowId) {
        event.eventType = FOCUS_IN;
        event.windowId = activeWindow;
        event.previousWindowId = windowId;
        event.timestamp = GetTickCount();
        
        SetActiveWindow(activeWindow);
    }
}
```

### Window Focus Validation

```c
bool ValidateWindowFocus(uint32_t windowId) {
    // Check if window exists
    if (!IsWindowValid(windowId)) {
        return false;
    }
    
    // Check if window can receive focus
    if (!g_WindowFocusStates[windowId].canFocus) {
        return false;
    }
    
    // Check if window is minimized
    if (IsWindowMinimized(windowId)) {
        return false;
    }
    
    return true;
}
```

### Focus Event Handling

```c
void HandleFocusEvent(FocusEvent* event) {
    switch (event->eventType) {
        case FOCUS_IN:
            // Window gained focus
            printf("Window focused: %d\n", event->windowId);
            UpdateActiveWindow(event->windowId);
            TriggerKeyboardInput(event->windowId);
            break;
            
        case FOCUS_OUT:
            // Window lost focus
            printf("Window unfocused: %d\n", event->windowId);
            DisableKeyboardInput(event->windowId);
            break;
    }
}
```

---

## Notes

- **Critical UI callback** for window management and navigation
- Handles keyboard (Tab key) and mouse interactions
- Provides detailed focus information including event types
- Should be registered early in initialization
- Focus state persists across multiple callbacks
- Handle focus/defocus events appropriately

---

## Security Considerations

- Validate window identifiers before processing
- Prevent focus stealing attacks
- Sanitize window state data
- Implement proper focus validation
- Handle malformed input gracefully

---

## Related Callbacks

- [OnInput](OnInput.md) - User input event callback
- [OnWindowEvent](OnWindowEvent.md) - Window event callback
- [OnResize](OnResize.md) - Window resize callback
- [OnClose](OnClose.md) - Window close callback

---

## VTable Functions

Related VTable functions for focus:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 2 | 0x08 | Reset | Reset focus state |
| 4 | 0x10 | RegisterCallback | Register callback |
| 5 | 0x14 | UnregisterCallback | Unregister callback |
| 6 | 0x18 | ProcessEvent | Process focus events |

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 5.2
- **Source**: `data_structures_analysis.md` WindowFocus section
- **Category**: UI event handling
- **Direction**: Launcher to Client callback invocation

---

**Status**: ✅ Documented  
**Confidence**: Medium (inferred from UI callback patterns)  
**Last Updated**: 2025-06-17