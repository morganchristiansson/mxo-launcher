# OnFocus

## Overview

**Category**: UI  
**Direction**: Launcher → Client  
**Purpose**: Window focus change notification callback - called when a window gains or loses focus

---

## Function Signature

```c
void OnFocus(FocusEvent* focusEvent, uint32_t windowId);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `FocusEvent*` | focusEvent | Focus event details and state |
| `uint32_t` | windowId | ID of the window that gained/lost focus |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value |

---

## Focus Event Types

The focus system handles various event types:

| Event Type | Description |
|------------|-------------|
| `FOCUS_GAINED` | Window gained focus (became active) |
| `FOCUS_LOST` | Window lost focus (another window became active) |
| `FOCUS_CHANGED` | Focus changed between windows |

---

## Focus Data Structures

### FocusEvent Structure

```c
struct FocusEvent {
    uint32_t eventType;        // Event type (FOCUS_GAINED, FOCUS_LOST, FOCUS_CHANGED)
    uint32_t prevWindowId;     // Previous focused window ID
    uint32_t newWindowId;      // New focused window ID
    uint32_t timestamp;        // Event timestamp
    uint32_t reason;           // Reason for focus change
};
```

**Size**: 20 bytes

### WindowObject Structure

```c
struct WindowObject {
    void* pVTable;              // 0x00: Virtual function table
    uint32_t windowId;          // 0x04: Unique window identifier
    uint32_t focusedState;      // 0x08: Focus state (1=focused, 0=not focused)
    
    // Window properties
    int32_t x;                  // 0x0C: X position
    int32_t y;                  // 0x10: Y position
    uint32_t width;             // 0x14: Width in pixels
    uint32_t height;            // 0x18: Height in pixels
    
    // Focus management
    uint32_t focusPriority;     // 0x1C: Focus priority for ordering
    uint32_t isActive;          // 0x20: Active window state
    uint32_t isVisible;         // 0x24: Visibility state
    uint32_t isMinimized;       // 0x28: Minimized state
    uint32_t isMaximized;       // 0x2C: Maximized state
    
    // Input handling
    void* inputCallback;        // 0x30: Registered input callback
    void* focusCallback;        // 0x34: Focus change callback pointer
    void* callbackUserData;     // 0x38: User data for callbacks
};
```

**Size**: 60 bytes

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
push windowId                 ; Window ID
push focusEvent              ; Focus event structure
call eax                      ; Invoke callback
add esp, 8                    ; Cleanup stack (2 parameters)

skip_callback:
```

---

## Implementation

### Launcher Side

```c
// Trigger focus change event
void OnFocusChanged(uint32_t prevWindowId, uint32_t newWindowId) {
    InputEvent event;
    event.eventType = INPUT_FOCUS_CHANGE;
    event.inputId = newWindowId;
    event.deltaX = 0;
    event.deltaY = 0;
    event.amount = 1;
    event.timestamp = GetTickCount();
    
    // Trigger callback
    OnInputReceived(&event);
}

void HandleWindowActivate(uint32_t windowId) {
    uint32_t prevWindowId = GetFocusedWindow();
    SetFocusedWindow(windowId);
    
    FocusEvent event;
    event.eventType = FOCUS_GAINED;
    event.prevWindowId = prevWindowId;
    event.newWindowId = windowId;
    event.timestamp = GetTickCount();
    event.reason = WINDOW_ACTIVATE;
    
    // Trigger callback
    OnFocusChanged(prevWindowId, windowId);
}

void HandleWindowDeactivate(uint32_t windowId) {
    uint32_t prevWindowId = windowId;
    uint32_t newWindowId = GetFocusedWindow();
    SetFocusedWindow(newWindowId);
    
    FocusEvent event;
    event.eventType = FOCUS_LOST;
    event.prevWindowId = prevWindowId;
    event.newWindowId = newWindowId;
    event.timestamp = GetTickCount();
    event.reason = WINDOW_DEACTIVATE;
    
    // Trigger callback
    OnFocusChanged(prevWindowId, windowId);
}

void HandleKeyboardFocus(uint32_t key) {
    switch (key) {
        case VK_TAB:
            // Tab forward/backward focus cycling
            CycleFocusForward();
            break;
            
        case VK_F4:
            if (GetModifierKey(VK_CONTROL)) {
                // Alt+F4 - Close current window
                CloseCurrentWindow();
            } else {
                // F4 - Focus next window
                CycleFocusForward();
            }
            break;
    }
}

void CycleFocusForward() {
    uint32_t focusedWindowId = GetFocusedWindow();
    uint32_t nextWindowId = GetNextWindowInZOrder(focusedWindowId);
    
    if (nextWindowId != 0) {
        ActivateWindow(nextWindowId);
    }
}

void CycleFocusBackward() {
    uint32_t focusedWindowId = GetFocusedWindow();
    uint32_t prevWindowId = GetPreviousWindowInZOrder(focusedWindowId);
    
    if (prevWindowId != 0) {
        ActivateWindow(prevWindowId);
    }
}
```

### Client Side

```c
// Focus callback implementation
void MyOnFocusCallback(FocusEvent* focusEvent, uint32_t windowId) {
    switch (focusEvent->eventType) {
        case FOCUS_GAINED:
            HandleWindowGainedFocus(focusEvent->newWindowId);
            break;
            
        case FOCUS_LOST:
            HandleWindowLostFocus(focusEvent->prevWindowId);
            break;
            
        case FOCUS_CHANGED:
            HandleFocusChanged(focusEvent->prevWindowId, focusEvent->newWindowId);
            break;
    }
}

// Window gained focus handler
void HandleWindowGainedFocus(uint32_t windowId) {
    WindowObject* window = GetWindow(windowId);
    
    // Update window state
    window->focusedState = 1;
    window->isActive = 1;
    
    // Clear selection in other windows
    ClearAllSelections();
    
    // Update UI
    UpdateWindowUI(window);
    ShowWindowCursor(window);
    
    printf("Window %d gained focus\n", windowId);
}

// Window lost focus handler
void HandleWindowLostFocus(uint32_t windowId) {
    WindowObject* window = GetWindow(windowId);
    
    // Update window state
    window->focusedState = 0;
    window->isActive = 0;
    
    // Save cursor position
    SaveCursorPosition(window);
    
    printf("Window %d lost focus\n", windowId);
}

// Focus changed handler
void HandleFocusChanged(uint32_t prevWindowId, uint32_t newWindowId) {
    // Clear selection in previous window
    WindowObject* prevWindow = GetWindow(prevWindowId);
    ClearSelectionInWindow(prevWindow);
    
    // Update focus tracking
    g_LastFocusedWindow = prevWindowId;
    g_CurrentFocusedWindow = newWindowId;
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
    regFunc = (int (*)(APIObject*, CallbackRegistration*))vtable->functions[24];
    
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

// Focus cycling for window navigation
void FocusNextWindow() {
    uint32_t focusedWindowId = GetFocusedWindow();
    
    // Get list of windows in Z-order
    uint32_t* windows = GetWindowList();
    int count = GetWindowCount();
    
    int currentIndex = -1;
    for (int i = 0; i < count; i++) {
        if (windows[i] == focusedWindowId) {
            currentIndex = i;
            break;
        }
    }
    
    // Calculate next index (wrap around)
    int nextIndex = (currentIndex + 1) % count;
    
    // Activate next window
    if (nextIndex < count && windows[nextIndex] != 0) {
        ActivateWindow(windows[nextIndex]);
    }
}

void FocusPreviousWindow() {
    uint32_t focusedWindowId = GetFocusedWindow();
    
    // Get list of windows in Z-order
    uint32_t* windows = GetWindowList();
    int count = GetWindowCount();
    
    int currentIndex = -1;
    for (int i = 0; i < count; i++) {
        if (windows[i] == focusedWindowId) {
            currentIndex = i;
            break;
        }
    }
    
    // Calculate previous index (wrap around)
    int prevIndex = (currentIndex - 1 + count) % count;
    
    // Activate previous window
    if (prevIndex < count && windows[prevIndex] != 0) {
        ActivateWindow(windows[prevIndex]);
    }
}
```

---

## Focus Key Codes

| Code | Description | Windows Key |
|------|-------------|-------------|
| VK_TAB (0x09) | Tab forward/backward | TAB |
| VK_F4 (0x1C) | Alt+F4 close, F4 next window | ALT+F4 / F4 |
| VK_CONTROL (0x11) | Control key modifier | CTRL |

---

## Focus Modifiers

| Flag | Description |
|------|-------------|
| 1 | Shift key pressed (reverse tab order) |
| 2 | Ctrl key pressed (special focus commands) |
| 4 | Alt key pressed (Alt+F4 close window) |
| 8 | Windows key pressed |

---

## Focus Flow

### Typical Focus Sequence

```
1. User interacts with UI
   └─> Clicks window/tab or presses keyboard shortcut

2. Launcher processes focus change
   └─> Updates internal state
   └─> Creates FocusEvent structure

3. Launcher triggers callback
   └─> Calls registered OnFocus function
   └─> Passes event data

4. Client processes event
   └─> Updates window selection
   └─> Handles specific focus change type
   └─> Updates UI state

5. Event cleanup
   └─> Clear selections in previous window
   └─> Update cursor visibility
```

---

## Focus Management

### Window Z-Order

Windows are ordered by their Z-order (stacking order):

```
Window 1 (topmost)
Window 2
Window 3
...
Window N (bottommost)
```

Focus typically cycles through windows in Z-order.

### Focus Priority

Each window can have a focus priority that affects the order:

```c
void SetWindowFocusPriority(uint32_t windowId, uint32_t priority) {
    WindowObject* window = GetWindow(windowId);
    window->focusPriority = priority;
}

uint32_t GetNextWindowInZOrder(uint32_t currentWindowId) {
    // Sort windows by focus priority
    // Return next window with higher priority
}

uint32_t GetPreviousWindowInZOrder(uint32_t currentWindowId) {
    // Sort windows by focus priority
    // Return previous window with lower priority
}
```

---

## Notes

- **Critical UI callback** for window navigation and interaction
- Called when windows gain/lose focus through user actions
- Provides detailed event information including reason for change
- Should be registered early in initialization
- Focus state persists across multiple callbacks
- Tab key typically cycles through focused windows
- Handle focus changes appropriately for UI updates

---

## Security Considerations

- Validate window IDs before processing
- Prevent focus stealing vulnerabilities
- Sanitize window identifiers
- Implement focus rate limiting
- Handle malformed focus events gracefully

---

## Related Callbacks

- [OnInput](OnInput.md) ([ui/OnInput.md](ui/OnInput.md)) - User input event callback
- [OnWindowEvent](OnWindowEvent.md) ([ui/OnWindowEvent.md](ui/OnWindowEvent.md)) - Window event callback
- [OnResize](OnResize.md) ([ui/OnResize.md](ui/OnResize.md)) - Window resize callback
- [OnClose](OnClose.md) ([ui/OnClose.md](ui/OnClose.md)) - Window close callback

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
- **Category**: UI event handling
- **Direction**: Launcher to Client callback invocation

---

**Status**: ⏳ Partial  
**Confidence**: Medium (inferred from UI callback patterns)  
**Last Updated**: 2026-03-08
**Documented By**: API_ANALYST

---

## TODO

- [ ] Find exact function signature in disassembly
- [ ] Locate FocusEvent structure in memory
- [ ] Identify focus event constants/enum values
- [ ] Search for diagnostic strings related to focus
- [ ] Verify callback registration method
- [ ] Document focus key handling patterns
- [ ] Add implementation examples based on actual code

---

**End of Template**